const { Gdk, Gio, GLib, GObject, Gst, GstPlayer, Gtk } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const { PlayerBase } = imports.clapper_src.playerBase;

let { debug } = Debug;
let { settings } = Misc;

var Player = GObject.registerClass(
class ClapperPlayer extends PlayerBase
{
    _init()
    {
        super._init();

        this.cursorInPlayer = false;
        this.is_local_file = false;
        this.seek_done = true;
        this.dragAllowed = false;
        this.isWidgetDragging = false;
        this.doneStartup = false;

        this.playOnFullscreen = false;
        this.quitOnStop = false;

        this.posX = 0;
        this.posY = 0;
        this.keyPressCount = 0;

        this._maxVolume = Misc.getLinearValue(Misc.maxVolume);

        this._playlist = [];
        this._trackId = 0;

        this._hideCursorTimeout = null;
        this._hideControlsTimeout = null;
        this._updateTimeTimeout = null;

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

        this.connect('state-changed', this._onStateChanged.bind(this));
        this.connect('uri-loaded', this._onUriLoaded.bind(this));
        this.connect('end-of-stream', this._onStreamEnded.bind(this));
        this.connect('warning', this._onPlayerWarning.bind(this));
        this.connect('error', this._onPlayerError.bind(this));

        this._realizeSignal = this.widget.connect('realize', this._onWidgetRealize.bind(this));
    }

    set_media(source)
    {
        let file;

        if(source.get_path)
            file = source;
        else {
            if(!Gst.uri_is_valid(source))
                source = Gst.filename_to_uri(source);

            if(!source)
                return debug('parsing source to URI failed');

            debug(`parsed source to URI: ${source}`);

            if(Gst.Uri.get_protocol(source) !== 'file') {
                this.is_local_file = false;
                return this.set_uri(source);
            }

            file = Gio.file_new_for_uri(source);
        }

        if(!file.query_exists(null)) {
            debug(`file does not exist: ${file.get_path()}`, 'LEVEL_WARNING');
            this._trackId++;

            if(this._playlist.length <= this._trackId)
                return debug('set media reached end of playlist');

            return this.set_media(this._playlist[this._trackId]);
        }

        let fileUri = file.get_uri();
        if(fileUri.endsWith('.claps'))
            return this.load_playlist_file(file);

        this.is_local_file = true;
        this.set_uri(fileUri);
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

    set_subtitles(source)
    {
        let uri = (source.get_uri)
            ? source.get_uri()
            : source;

        this.set_subtitle_uri(uri);
        this.set_subtitle_track_enabled(true);

        debug(`applied subtitle track: ${uri}`);
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
        this.seek_done = false;

        if(this.state === GstPlayer.PlayerState.STOPPED)
            this.pause();

        if(position < 0)
            position = 0;

        debug(`${this.seekingMode} seeking to position: ${position}`);

        super.seek(position);
    }

    seek_seconds(position)
    {
        this.seek(position * 1000000000);
    }

    set_volume(volume)
    {
        if(volume < 0)
            volume = 0;
        else if(volume > this._maxVolume)
            volume = this._maxVolume;

        super.set_volume(volume);
        debug(`set player volume: ${volume}`);
    }

    adjust_position(isIncrease)
    {
        this.seek_done = false;

        let { controls } = this.widget.get_ancestor(Gtk.Grid);
        let max = controls.positionAdjustment.get_upper();
        let seekingValue = settings.get_int('seeking-value');
        let seekingUnit = settings.get_string('seeking-unit');

        switch(seekingUnit) {
            case 'minute':
                seekingValue *= 60;
                break;
            case 'percentage':
                seekingValue = max * seekingValue / 100;
                break;
            default:
                break;
        }

        if(!isIncrease)
            seekingValue *= -1;

        let positionSeconds = controls.positionScale.get_value() + seekingValue;

        if(positionSeconds > max)
            positionSeconds = max;

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

    receiveWs(action, value)
    {
        switch(action) {
            case 'toggle_play':
            case 'play':
            case 'pause':
            case 'set_playlist':
                this[action](value);
                break;
            default:
                super.receiveWs(action, value);
                break;
        }
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
                if(clapperWidget.fullscreenMode || clapperWidget.floatingMode) {
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

    _performCloseCleanup(window)
    {
        window.disconnect(this.closeRequestSignal);
        this.closeRequestSignal = null;

        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        if(!clapperWidget.fullscreenMode) {
            let size = (Misc.isOldGtk)
                ? window.get_size()
                : window.get_default_size();

            if(size[0] > 0 && size[1] > 0)
                clapperWidget._saveWindowSize(size);
        }
        settings.set_double('volume-last', this.volume);

        clapperWidget.controls._onCloseRequest();
    }

    _onStateChanged(player, state)
    {
        this.state = state;
        this.emitWs('state_changed', state);

        if(state !== GstPlayer.PlayerState.BUFFERING) {
            let root = player.widget.get_root();
            Misc.inhibitForState(state, root);

            if(this.quitOnStop) {
                if(state === GstPlayer.PlayerState.STOPPED)
                    root.run_dispose();

                return;
            }
        }

        let clapperWidget = player.widget.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        if(!this.seek_done && state !== GstPlayer.PlayerState.BUFFERING) {
            clapperWidget.updateTime();
            this.seek_done = true;
            debug('seeking finished');
        }

        clapperWidget._onPlayerStateChanged(player, state);
    }

    _onStreamEnded(player)
    {
        debug(`end of stream: ${this._trackId}`);
        this.emitWs('end_of_stream', this._trackId);

        this._trackId++;

        if(this._trackId < this._playlist.length)
            this.set_media(this._playlist[this._trackId]);
        else if(settings.get_boolean('close-auto')) {
            /* Stop will be automatically called soon afterwards */
            this._performCloseCleanup(this.widget.get_root());
            this.quitOnStop = true;
        }
    }

    _onUriLoaded(player, uri)
    {
        debug(`URI loaded: ${uri}`);

        if(!this.doneStartup) {
            this.doneStartup = true;

            if(settings.get_boolean('fullscreen-auto')) {
                let root = player.widget.get_root();
                let clapperWidget = root.get_child();
                if(!clapperWidget.fullscreenMode) {
                    this.playOnFullscreen = true;
                    root.fullscreen();

                    return;
                }
            }
        }
        this.play();
    }

    _onPlayerWarning(player, error)
    {
        debug(error.message, 'LEVEL_WARNING');
    }

    _onPlayerError(player, error)
    {
        debug(error);
    }

    _onWidgetRealize()
    {
        this.widget.disconnect(this._realizeSignal);
        this._realizeSignal = null;

        let root = this.widget.get_root();
        if(!root) return;

        this.closeRequestSignal = root.connect('close-request', this._onCloseRequest.bind(this));
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
                this.adjust_position(bool);
                this._clearTimeout('hideControls');
                if(this.keyPressCount > 1) {
                    clapperWidget.revealerBottom.set_can_focus(false);
                    clapperWidget.revealerBottom.revealChild(true);
                }
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
                break;
            case Gdk.KEY_F11:
            case Gdk.KEY_f:
            case Gdk.KEY_F:
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
        this.isWidgetDragging = false;

        this._setHideCursorTimeout();

        let clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        if(clapperWidget.fullscreenMode || clapperWidget.floatingMode)
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

            if(clapperWidget.floatingMode && !clapperWidget.fullscreenMode) {
                clapperWidget.revealerBottom.set_can_focus(false);
                clapperWidget.revealerBottom.revealChild(true);
                this._setHideControlsTimeout();
            }
            else if(clapperWidget.fullscreenMode) {
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

            let native = this.widget.get_native();
            if(!native) return;

            let [isShared, winX, winY] = this.widget.translate_coordinates(
                native, startX, startY
            );
            if(!isShared) return;

            let [nativeX, nativeY] = native.get_surface_transform();
            winX += nativeX;
            winY += nativeY;

            this.isWidgetDragging = true;
            native.get_surface().begin_move(
                gesture.get_device(),
                gesture.get_current_button(),
                winX,
                winY,
                gesture.get_current_event_time()
            );

            gesture.reset();
        }
    }

    _onScroll(controller, dx, dy)
    {
        let isHorizontal = Math.abs(dx) >= Math.abs(dy);
        let isIncrease = (isHorizontal) ? dx < 0 : dy < 0;

        if(isHorizontal) {
            this.adjust_position(isIncrease);
            let { controls } = this.widget.get_ancestor(Gtk.Grid);
            let value = Math.round(controls.positionScale.get_value());
            this.seek_seconds(value);
        }
        else
            this.adjust_volume(isIncrease);

        return true;
    }

    _onCloseRequest(window)
    {
        this._performCloseCleanup(window);

        if(this.state === GstPlayer.PlayerState.STOPPED)
            return window.run_dispose();

        this.quitOnStop = true;
        this.stop();
    }
});
