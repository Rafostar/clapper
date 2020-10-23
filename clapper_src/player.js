const { Gdk, Gio, GLib, GObject, Gst, GstPlayer, Gtk } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.clapper_src.debug;

const GSTPLAYER_DEFAULTS = {
    position_update_interval: 1000,
    seek_accurate: false,
    user_agent: 'clapper',
};

let { debug } = Debug;

var Player = GObject.registerClass(
class ClapperPlayer extends GstPlayer.Player
{
    _init(opts)
    {
        opts = opts || {};
        opts = Object.assign({}, GSTPLAYER_DEFAULTS, opts);

        if(!Gst.is_initialized())
            Gst.init(null);

        let plugin = 'gtk4glsink';
        let gtkglsink = Gst.ElementFactory.make(plugin, null);

        if(!gtkglsink) {
            return debug(new Error(
                `Could not load "${plugin}".`
                    + ' Do you have gstreamer-plugins-good-gtk4 installed?'
            ));
        }

        let glsinkbin = Gst.ElementFactory.make('glsinkbin', null);
        glsinkbin.sink = gtkglsink;

        let dispatcher = new GstPlayer.PlayerGMainContextSignalDispatcher();
        let renderer = new GstPlayer.PlayerVideoOverlayVideoRenderer({
            video_sink: glsinkbin
        });

        super._init({
            signal_dispatcher: dispatcher,
            video_renderer: renderer
        });

        this.is_local_file = false;
        this.seek_done = true;
        this.dragAllowed = false;

        this.posX = 0;
        this.posY = 0;
        this.keyPressCount = 0;

        this._playerSignals = [];
        this._widgetSignals = [];

        let config = this.get_config();

        for(let setting of Object.keys(GSTPLAYER_DEFAULTS)) {
            let setOption = GstPlayer.Player[`config_set_${setting}`];
            if(!setOption) {
                debug(`unsupported option: ${setting}`, 'LEVEL_WARNING');
                continue;
            }
            setOption(config, opts[setting]);
        }

        this.set_config(config);
        this.set_mute(false);
        this.set_plugin_rank('vah264dec', 300);

        this.widget = gtkglsink.widget;
        this.widget.vexpand = true;
        this.widget.hexpand = true;

        this.state = GstPlayer.PlayerState.STOPPED;
        this.visualization_enabled = false;
        this.fast_seeking = opts.fast_seeking || false;

        this._playlist = [];
        this._trackId = 0;

        this._hideCursorTimeout = null;
        this._hideControlsTimeout = null;
        this._updateTimeTimeout = null;

        this.cursorInPlayer = false;

        let clickGesture = new Gtk.GestureClick();
        clickGesture.set_button(0);
        clickGesture.connect('pressed', this._onWidgetPressed.bind(this));
        this.widget.add_controller(clickGesture);

        let dragGesture = new Gtk.GestureDrag();
        dragGesture.connect('drag-update', this._onWidgetDragUpdate.bind(this));
        this.widget.add_controller(dragGesture);

        let keyController = new Gtk.EventControllerKey();
        keyController.connect('key-pressed', this._onWidgetKeyPressed.bind(this));
        keyController.connect('key-released', this._onWidgetKeyReleased.bind(this));
        this.widget.add_controller(keyController);

        let scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(Gtk.EventControllerScrollFlags.BOTH_AXES);
        scrollController.connect('scroll', this._onScroll.bind(this));
        this.widget.add_controller(scrollController);

        let motionController = new Gtk.EventControllerMotion();
        motionController.connect('enter', this._onWidgetEnter.bind(this));
        motionController.connect('leave', this._onWidgetLeave.bind(this));
        motionController.connect('motion', this._onWidgetMotion.bind(this));
        this.widget.add_controller(motionController);

        this.selfConnect('state-changed', this._onStateChanged.bind(this));
        this.selfConnect('uri-loaded', this._onUriLoaded.bind(this));
        this.selfConnect('end-of-stream', this._onStreamEnded.bind(this));
        this.selfConnect('warning', this._onPlayerWarning.bind(this));
        this.selfConnect('error', this._onPlayerError.bind(this));

        this._realizeSignal = this.widget.connect('realize', this._onWidgetRealize.bind(this));
    }

    set_media(source)
    {
        if(!Gst.uri_is_valid(source))
            source = Gst.filename_to_uri(source);

        if(!source)
            return debug('parsing source to URI failed');

        debug(`parsed source to URI: ${source}`);

        if(Gst.Uri.get_protocol(source) !== 'file') {
            this.is_local_file = false;
            return this.set_uri(source);
        }

        let file = Gio.file_new_for_uri(source);

        if(!file.query_exists(null)) {
            debug(`file does not exist: ${source}`, 'LEVEL_WARNING');
            this._trackId++;

            if(this._playlist.length <= this._trackId)
                return debug('set media reached end of playlist');

            return this.set_media(this._playlist[this._trackId]);
        }

        if(file.get_path().endsWith('.claps'))
            return this.load_playlist_file(file);

        this.is_local_file = true;
        this.set_uri(source);
    }

    load_playlist_file(file)
    {
        let stream = new Gio.DataInputStream({
            base_stream: file.read(null)
        });
        let listdir = file.get_parent();
        let playlist = [];
        let line;

        while((line = stream.read_line(null)[0])) {
            line = (line instanceof Uint8Array)
                ? ByteArray.toString(line).trim()
                : String(line).trim();

            if(!Gst.uri_is_valid(line)) {
                let lineFile = listdir.resolve_relative_path(line);
                if(!lineFile)
                    continue;

                line = lineFile.get_path();
            }
            debug(`new playlist item: ${line}`);
            playlist.push(line);
        }
        stream.close(null);
        this.set_playlist(playlist);
    }

    set_playlist(playlist)
    {
        if(!Array.isArray(playlist) || !playlist.length)
            return;

        this._trackId = 0;
        this._playlist = playlist;

        this.set_media(this._playlist[0]);
    }

    get_playlist()
    {
        return this._playlist;
    }

    set_visualization_enabled(value)
    {
        if(value === this.visualization_enabled)
            return;

        super.set_visualization_enabled(value);
        this.visualization_enabled = value;
    }

    get_visualization_enabled()
    {
        return this.visualization_enabled;
    }

    seek(position)
    {
        if(this.state === GstPlayer.PlayerState.STOPPED)
            this.pause();

        if(position < 0)
            position = 0;

        this.seek_done = false;
        debug(`player is seeking to position: ${position}`);

        if(!this.fast_seeking)
            return super.seek(position);

        let pipeline = this.get_pipeline();
        let flags = Gst.SeekFlags.FLUSH
            | Gst.SeekFlags.KEY_UNIT
            | Gst.SeekFlags.SNAP_AFTER;

        pipeline.seek_simple(Gst.Format.TIME, flags, position);
    }

    seek_seconds(position)
    {
        this.seek(position * 1000000000);
    }

    set_volume(volume)
    {
        if(volume < 0)
            volume = 0;
        else if(volume > 2)
            volume = 2;

        super.set_volume(volume);
    }

    adjust_position(isIncrease)
    {
        let { controls } = this.widget.get_ancestor(Gtk.Grid);

        let value = (isIncrease) ? 10 : -10;
        let positionSeconds = controls.positionScale.get_value() + value;
        controls.positionScale.set_value(positionSeconds);
    }

    adjust_volume(isIncrease)
    {
        let { controls } = this.widget.get_ancestor(Gtk.Grid);

        let value = (isIncrease) ? 0.05 : -0.05;
        let volume = controls.volumeScale.get_value() + value;
        controls.volumeScale.set_value(volume);
    }

    toggle_play()
    {
        let action = (this.state === GstPlayer.PlayerState.PLAYING)
            ? 'pause'
            : 'play';

        this[action]();
    }

    set_subtitle_font_desc(desc)
    {
        let pipeline = this.get_pipeline();
        pipeline.subtitle_font_desc = desc;
    }

    set_plugin_rank(name, rank)
    {
        debug(`changing rank of plugin: ${name}`);

        let gstRegistry = Gst.Registry.get();
        let feature = gstRegistry.lookup_feature(name);
        if(!feature)
            return debug(`plugin unavailable: ${name}`);

        let oldRank = feature.get_rank();
        feature.set_rank(rank);

        debug(`changed rank: ${oldRank} -> ${rank} for ${name}`);
    }

    selfConnect(signal, fn)
    {
        this._playerSignals.push(
            super.connect(signal, fn)
        );
    }

    _setHideCursorTimeout()
    {
        this._clearTimeout('hideCursor');
        this._hideCursorTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 1, () => {
            this._hideCursorTimeout = null;

            if(this.cursorInPlayer) {
                let blankCursor = Gdk.Cursor.new_from_name('none', null);
                this.widget.set_cursor(blankCursor);
            }

            return GLib.SOURCE_REMOVE;
        });
    }

    _setHideControlsTimeout()
    {
        this._clearTimeout('hideControls');
        this._hideControlsTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 3, () => {
            this._hideControlsTimeout = null;

            if(this.cursorInPlayer) {
                let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
                if(clapperWidget.fullscreenMode) {
                    this._clearTimeout('updateTime');
                    clapperWidget.revealControls(false);
                }
            }

            return GLib.SOURCE_REMOVE;
        });
    }

    _setUpdateTimeInterval()
    {
        this._clearTimeout('updateTime');
        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        let nextUpdate = clapperWidget.updateTime();

        if(nextUpdate === null)
            return;

        this._updateTimeTimeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, nextUpdate, () => {
            this._updateTimeTimeout = null;

            if(clapperWidget.fullscreenMode)
                this._setUpdateTimeInterval();

            return GLib.SOURCE_REMOVE;
        });
    }

    _clearTimeout(name)
    {
        if(!this[`_${name}Timeout`])
            return;

        GLib.source_remove(this[`_${name}Timeout`]);
        this[`_${name}Timeout`] = null;

        if(name === 'updateTime')
            debug('cleared update time interval');
    }

    _onStateChanged(player, state)
    {
        this.state = state;

        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        if(!this.seek_done && this.state !== GstPlayer.PlayerState.BUFFERING) {
            clapperWidget.updateTime();
            this.seek_done = true;
            debug('seeking finished');
        }

        clapperWidget._onPlayerStateChanged(player, state);
    }

    _onStreamEnded(player)
    {
        this._trackId++;

        if(this._trackId < this._playlist.length)
            this.set_media(this._playlist[this._trackId]);
        else
            this.stop();
    }

    _onUriLoaded(self, uri)
    {
        this.play();
        debug(`URI loaded: ${uri}`);
    }

    _onPlayerWarning(self, error)
    {
        debug(error.message, 'LEVEL_WARNING');
    }

    _onPlayerError(self, error)
    {
        debug(error);
    }

    _onWidgetRealize()
    {
        this.widget.disconnect(this._realizeSignal);
        this._realizeSignal = null;

        let root = this.widget.get_root();
        if(!root) return;

        root.connect('close-request', this._onCloseRequest.bind(this));
    }

    /* Widget only - does not happen when using controls navigation */
    _onWidgetKeyPressed(controller, keyval, keycode, state)
    {
        this.keyPressCount++;

        let bool = false;
        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);

        switch(keyval) {
            case Gdk.KEY_Up:
                bool = true;
            case Gdk.KEY_Down:
                this.adjust_volume(bool);
                break;
            case Gdk.KEY_Right:
                bool = true;
            case Gdk.KEY_Left:
                clapperWidget.controls.isPositionSeeking = true;
                this._clearTimeout('hideControls');
                if(this.keyPressCount > 1) {
                    clapperWidget.revealerBottom.set_can_focus(false);
                    clapperWidget.revealerBottom.revealChild(true);
                }
                this.adjust_position(bool);
                break;
            default:
                break;
        }
    }

    /* Also happens after using controls navigation for selected keys */
    _onWidgetKeyReleased(controller, keyval, keycode, state)
    {
        this.keyPressCount = 0;

        let value;
        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);

        switch(keyval) {
            case Gdk.KEY_space:
                this.toggle_play();
                break;
            case Gdk.KEY_Return:
                if(clapperWidget.fullscreenMode) {
                    clapperWidget.revealControls(true);
                    this._setHideControlsTimeout();
                }
                break;
            case Gdk.KEY_Right:
            case Gdk.KEY_Left:
                value = Math.round(
                    clapperWidget.controls.positionScale.get_value()
                );
                this.seek_seconds(value);
                this._setHideControlsTimeout();
                clapperWidget.controls.isPositionSeeking = false;
                break;
            case Gdk.KEY_F11:
                clapperWidget.toggleFullscreen();
                break;
            case Gdk.KEY_Escape:
                if(clapperWidget.fullscreenMode) {
                    let root = this.widget.get_root();
                    root.unfullscreen();
                }
                break;
            case Gdk.KEY_q:
            case Gdk.KEY_Q:
                let root = this.widget.get_root();
                root.emit('close-request');
                root.destroy();
                break;
            default:
                break;
        }
    }

    _onWidgetPressed(gesture, nPress, x, y)
    {
        let button = gesture.get_current_button();
        let isDouble = (nPress % 2 == 0);
        this.dragAllowed = !isDouble;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                if(isDouble) {
                    let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
                    clapperWidget.toggleFullscreen();
                }
                break;
            case Gdk.BUTTON_SECONDARY:
                this.toggle_play();
                break;
            default:
                break;
        }
    }

    _onWidgetEnter(controller, x, y)
    {
        this.cursorInPlayer = true;

        this._setHideCursorTimeout();

        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        if(clapperWidget.fullscreenMode)
            this._setHideControlsTimeout();
    }

    _onWidgetLeave(controller)
    {
        this.cursorInPlayer = false;

        this._clearTimeout('hideCursor');
        this._clearTimeout('hideControls');
    }

    _onWidgetMotion(controller, posX, posY)
    {
        this.cursorInPlayer = true;

        /* GTK4 sometimes generates motions with same coords */
        if(this.posX === posX && this.posY === posY)
            return;

        /* Do not show cursor on small movements */
        if(
            Math.abs(this.posX - posX) >= 0.5
            || Math.abs(this.posY - posY) >= 0.5
        ) {
            let defaultCursor = Gdk.Cursor.new_from_name('default', null);
            this.widget.set_cursor(defaultCursor);
            this._setHideCursorTimeout();

            let clapperWidget = this.widget.get_ancestor(Gtk.Grid);

            if(clapperWidget.fullscreenMode) {
                if(!this._updateTimeTimeout)
                    this._setUpdateTimeInterval();

                if(!clapperWidget.revealerTop.get_reveal_child()) {
                    /* Do not grab controls key focus on mouse movement */
                    clapperWidget.revealerBottom.set_can_focus(false);
                    clapperWidget.revealControls(true);
                }
                this._setHideControlsTimeout();
            }
            else {
                if(this._hideControlsTimeout)
                    this._clearTimeout('hideControls');
                if(this._updateTimeTimeout)
                    this._clearTimeout('updateTime');
            }
        }

        this.posX = posX;
        this.posY = posY;
    }

    _onWidgetDragUpdate(gesture, offsetX, offsetY)
    {
        if(!this.dragAllowed)
            return;

        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        if(clapperWidget.fullscreenMode)
            return;

        let { gtk_double_click_distance } = this.widget.get_settings();

        if (
            Math.abs(offsetX) > gtk_double_click_distance
            || Math.abs(offsetY) > gtk_double_click_distance
        ) {
            let [isActive, startX, startY] = gesture.get_start_point();
            if(!isActive) return;

            let root = this.widget.get_root();
            if(!root) return;

            root.get_surface().begin_move(
                gesture.get_device(),
                gesture.get_current_button(),
                startX,
                startY,
                gesture.get_current_event_time()
            );

            gesture.reset();
        }
    }

    _onScroll(controller, dx, dy)
    {
        let isHorizontal = Math.abs(dx) >= Math.abs(dy);
        let isIncrease = (isHorizontal) ? dx < 0 : dy < 0;

        if(isHorizontal)
            this.adjust_position(isIncrease);
        else
            this.adjust_volume(isIncrease);

        return true;
    }

    _onCloseRequest(window)
    {
        while(this._widgetSignals.length)
            this.widget.disconnect(this._widgetSignals.pop());

        while(this._playerSignals.length)
            this.disconnect(this._playerSignals.pop());

        if(this.state !== GstPlayer.PlayerState.STOPPED)
            this.stop();
    }
});
