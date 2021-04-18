const { Gdk, Gio, GLib, GObject, Gst, GstClapper, Gtk } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const YouTube = imports.src.youtube;
const { PlaylistWidget } = imports.src.playlist;
const { WebApp } = imports.src.webApp;

const { debug } = Debug;
const { settings } = Misc;

let WebServer;

var Player = GObject.registerClass(
class ClapperPlayer extends GstClapper.Clapper
{
    _init()
    {
        const gtk4plugin = new GstClapper.ClapperGtk4Plugin();
        const glsinkbin = Gst.ElementFactory.make('glsinkbin', null);
        glsinkbin.sink = gtk4plugin.video_sink;

        const dispatcher = new GstClapper.ClapperGMainContextSignalDispatcher();
        const renderer = new GstClapper.ClapperVideoOverlayVideoRenderer({
            video_sink: glsinkbin
        });

        super._init({
            signal_dispatcher: dispatcher,
            video_renderer: renderer
        });

        this.widget = gtk4plugin.video_sink.widget;
        this.widget.add_css_class('videowidget');

        this.state = GstClapper.ClapperState.STOPPED;
        this.visualization_enabled = false;

        this.webserver = null;
        this.webapp = null;
        this.playlistWidget = new PlaylistWidget();

        this.seek_done = true;
        this.needsFastSeekRestore = false;
        this.customVideoTitle = null;

        this.windowMapped = false;
        this.canAutoFullscreen = false;
        this.playOnFullscreen = false;
        this.quitOnStop = false;
        this.needsTocUpdate = true;

        this.keyPressCount = 0;
        this.ytClient = null;

        const keyController = new Gtk.EventControllerKey();
        keyController.connect('key-pressed', this._onWidgetKeyPressed.bind(this));
        keyController.connect('key-released', this._onWidgetKeyReleased.bind(this));
        this.widget.add_controller(keyController);

        this.set_all_plugins_ranks();
        this.set_initial_config();
        this.set_and_bind_settings();

        this.connect('state-changed', this._onStateChanged.bind(this));
        this.connect('uri-loaded', this._onUriLoaded.bind(this));
        this.connect('end-of-stream', this._onStreamEnded.bind(this));
        this.connect('warning', this._onPlayerWarning.bind(this));
        this.connect('error', this._onPlayerError.bind(this));

        settings.connect('changed', this._onSettingsKeyChanged.bind(this));

        this._realizeSignal = this.widget.connect('realize', this._onWidgetRealize.bind(this));
    }

    set_and_bind_settings()
    {
        const settingsToSet = [
            'seeking-mode',
            'audio-offset',
            'subtitle-offset',
            'play-flags',
            'webserver-enabled'
        ];

        for(let key of settingsToSet)
            this._onSettingsKeyChanged(settings, key);

        const flag = Gio.SettingsBindFlags.GET;
        settings.bind('subtitle-font', this.pipeline, 'subtitle_font_desc', flag);
    }

    set_initial_config()
    {
        this.set_mute(false);

        /* FIXME: change into option in preferences */
        const pipeline = this.get_pipeline();
        pipeline.ring_buffer_max_size = 8 * 1024 * 1024;
    }

    set_all_plugins_ranks()
    {
        let data = [];

        /* Set empty plugin list if someone messed it externally */
        try {
            data = JSON.parse(settings.get_string('plugin-ranking'));
            if(!Array.isArray(data))
                throw new Error('plugin ranking data is not an array!');
        }
        catch(err) {
            debug(err);
            settings.set_string('plugin-ranking', "[]");
        }

        for(let plugin of data) {
            if(!plugin.apply || !plugin.name)
                continue;

            this.set_plugin_rank(plugin.name, plugin.rank);
        }
    }

    set_plugin_rank(name, rank)
    {
        const gstRegistry = Gst.Registry.get();
        const feature = gstRegistry.lookup_feature(name);
        if(!feature)
            return debug(`plugin unavailable: ${name}`);

        const oldRank = feature.get_rank();
        if(rank === oldRank)
            return;

        feature.set_rank(rank);
        debug(`changed rank: ${oldRank} -> ${rank} for ${name}`);
    }

    draw_black(isEnabled)
    {
        this.widget.ignore_textures = isEnabled;

        if(this.state !== GstClapper.ClapperState.PLAYING)
            this.widget.queue_render();
    }

    set_uri(uri)
    {
        this.customVideoTitle = null;

        if(Gst.Uri.get_protocol(uri) !== 'file') {
            const [isYouTubeUri, videoId] = YouTube.checkYouTubeUri(uri);

            if(!isYouTubeUri)
                return super.set_uri(uri);

            if(!this.ytClient)
                this.ytClient = new YouTube.YouTubeClient();

            const { root } = this.widget;
            const surface = root.get_surface();
            const monitor = root.display.get_monitor_at_surface(surface);

            this.ytClient.getPlaybackDataAsync(videoId, monitor)
                .then(data => {
                    this.customVideoTitle = data.title;
                    super.set_uri(data.uri);
                })
                .catch(debug);

            return;
        }

        const file = Misc.getFileFromLocalUri(uri);
        if(!file) {
            if(!this.playlistWidget.nextTrack())
                debug('set media reached end of playlist');

            return;
        }
        if(uri.endsWith('.claps')) {
            this.load_playlist_file(file);

            return;
        }

        super.set_uri(uri);
    }

    load_playlist_file(file)
    {
        const stream = new Gio.DataInputStream({
            base_stream: file.read(null)
        });
        const listdir = file.get_parent();
        const playlist = [];

        let line;
        while((line = stream.read_line(null)[0])) {
            line = (line instanceof Uint8Array)
                ? ByteArray.toString(line).trim()
                : String(line).trim();

            if(!Gst.uri_is_valid(line)) {
                const lineFile = listdir.resolve_relative_path(line);
                if(!lineFile)
                    continue;

                line = lineFile.get_uri();
            }
            debug(`new playlist item: ${line}`);
            playlist.push(line);
        }
        stream.close(null);
        this.set_playlist(playlist);
    }

    set_playlist(playlist)
    {
        if(this.state !== GstClapper.ClapperState.STOPPED)
            this.stop();

        this.playlistWidget.removeAll();
        this.canAutoFullscreen = true;

        for(let source of playlist) {
            const uri = this._getSourceUri(source);
            this.playlistWidget.addItem(uri);
        }

        /* If not mapped yet, first track will play after map */
        if(this.windowMapped)
            this._playFirstTrack();
    }

    set_subtitles(source)
    {
        const uri = this._getSourceUri(source);

        /* Check local file existence */
        if(
            Gst.Uri.get_protocol(uri) === 'file'
            && !Misc.getFileFromLocalUri(uri)
        )
            return;

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
        /* avoid seek emits when position bar is altered */
        if(this.needsTocUpdate)
            return;

        this.seek_done = false;

        if(this.state === GstClapper.ClapperState.STOPPED)
            this.pause();

        if(position < 0)
            position = 0;

        debug(`${this.seekingMode} seeking to position: ${position}`);

        super.seek(position);
    }

    seek_seconds(seconds)
    {
        this.seek(seconds * Gst.SECOND);
    }

    seek_chapter(seconds)
    {
        if(this.seekingMode !== 'fast') {
            this.seek_seconds(seconds);
            return;
        }

        this.set_seek_mode(GstClapper.ClapperSeekMode.DEFAULT);
        this.seekingMode = 'normal';
        this.needsFastSeekRestore = true;

        this.seek_seconds(seconds);
    }

    adjust_position(isIncrease)
    {
        this.seek_done = false;

        const { controls } = this.widget.get_ancestor(Gtk.Grid);
        const max = controls.positionAdjustment.get_upper();
        const seekingUnit = settings.get_string('seeking-unit');

        let seekingValue = settings.get_int('seeking-value');

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

    adjust_volume(isIncrease, offset)
    {
        offset = offset || 0.05;

        const { controls } = this.widget.get_ancestor(Gtk.Grid);
        const value = (isIncrease) ? offset : -offset;
        const volume = controls.volumeScale.get_value() + value;

        controls.volumeScale.set_value(volume);
    }

    toggle_play()
    {
        const action = (this.state === GstClapper.ClapperState.PLAYING)
            ? 'pause'
            : 'play';

        this[action]();
    }

    emitWs(action, value)
    {
        if(!this.webserver)
            return;

        this.webserver.sendMessage({ action, value });
    }

    receiveWs(action, value)
    {
        switch(action) {
            case 'toggle_play':
            case 'play':
            case 'pause':
            case 'set_playlist':
            case 'set_subtitles':
                this[action](value);
                break;
            case 'toggle_maximized':
                action = 'toggle-maximized';
            case 'minimize':
            case 'close':
                this.widget.activate_action(`window.${action}`, null);
                break;
            default:
                const clapperWidget = this.widget.get_ancestor(Gtk.Grid);

                switch(action) {
                    case 'toggle_fullscreen':
                        clapperWidget.toggleFullscreen();
                        break;
                    default:
                        debug(`unhandled WebSocket action: ${action}`);
                        break;
                }
                break;
        }
    }

    _getSourceUri(source)
    {
        return (source.get_uri != null)
            ? source.get_uri()
            : Gst.uri_is_valid(source)
            ? source
            : Gst.filename_to_uri(source);
    }

    _playFirstTrack()
    {
        const firstTrack = this.playlistWidget.get_row_at_index(0);
        if(!firstTrack) return;

        firstTrack.activate();
    }

    _performCloseCleanup(window)
    {
        window.disconnect(this.closeRequestSignal);
        this.closeRequestSignal = null;

        const clapperWidget = this.widget.get_ancestor(Gtk.Grid);

        if(!clapperWidget.isFullscreenMode && clapperWidget.controlsRevealer.child_revealed) {
            const size = window.get_default_size();

            if(size[0] > 0 && size[1] > 0) {
                settings.set_string('window-size', JSON.stringify(size));
                debug(`saved window size: ${size[0]}x${size[1]}`);
            }
        }
        /* If "quitOnStop" is set here it means that we are in middle of autoclosing */
        if(this.state !== GstClapper.ClapperState.STOPPED && !this.quitOnStop) {
            const playlistItem = this.playlistWidget.getActiveRow();

            let resumeInfo = {};
            if(playlistItem.isLocalFile && settings.get_boolean('resume-enabled')) {
                const resumeTime = Math.floor(this.position / Gst.SECOND);
                const resumeDuration = this.duration / Gst.SECOND;

                /* Do not save resume info when video is very short,
                 * just started or almost finished */
                if(
                    resumeDuration > 60
                    && resumeTime > 15
                    && resumeDuration - resumeTime > 20
                ) {
                    resumeInfo.title = playlistItem.filename;
                    resumeInfo.time = resumeTime;
                    resumeInfo.duration = resumeDuration;

                    debug(`saving resume info for: ${resumeInfo.title}`);
                    debug(`resume time: ${resumeInfo.time}, duration: ${resumeInfo.duration}`);
                }
                else
                    debug('resume info is not worth saving');
            }
            settings.set_string('resume-database', JSON.stringify([resumeInfo]));
        }
        const volume = this.volume;
        debug(`saving last volume: ${volume}`);
        settings.set_double('volume-last', volume);

        clapperWidget.controls._onCloseRequest();
    }

    _onStateChanged(player, state)
    {
        this.state = state;
        this.emitWs('state_changed', state);

        if(state !== GstClapper.ClapperState.BUFFERING) {
            const root = player.widget.get_root();

            if(this.quitOnStop) {
                if(root && state === GstClapper.ClapperState.STOPPED)
                    root.run_dispose();

                return;
            }
            const isInhibit = (state === GstClapper.ClapperState.PLAYING);
            Misc.setAppInhibit(isInhibit, root);
        }

        const clapperWidget = player.widget.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        if(!this.seek_done && state !== GstClapper.ClapperState.BUFFERING) {
            clapperWidget.updateTime();

            if(this.needsFastSeekRestore) {
                this.set_seek_mode(GstClapper.ClapperSeekMode.FAST);
                this.seekingMode = 'fast';
                this.needsFastSeekRestore = false;
            }

            this.seek_done = true;
            debug('seeking finished');

            clapperWidget._onPlayerPositionUpdated(this, this.position);
        }

        clapperWidget._onPlayerStateChanged(player, state);
    }

    _onStreamEnded(player)
    {
        const lastTrackId = this.playlistWidget.activeRowId;

        debug(`end of stream: ${lastTrackId}`);
        this.emitWs('end_of_stream', lastTrackId);

        if(this.playlistWidget.nextTrack())
            return;

        if(settings.get_boolean('close-auto')) {
            /* Stop will be automatically called soon afterwards */
            this.quitOnStop = true;
            this._performCloseCleanup(this.widget.get_root());
        }
    }

    _onUriLoaded(player, uri)
    {
        debug(`URI loaded: ${uri}`);
        this.needsTocUpdate = true;

        if(this.canAutoFullscreen) {
            this.canAutoFullscreen = false;

            if(settings.get_boolean('fullscreen-auto')) {
                const root = player.widget.get_root();
                const clapperWidget = root.get_child();
                /* Do not enter fullscreen when already in it
                 * or when in floating mode */
                if(
                    !clapperWidget.isFullscreenMode
                    && clapperWidget.controlsRevealer.reveal_child
                ) {
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
        debug(error.message);
    }

    _onPlayerError(player, error)
    {
        debug(error);
    }

    _onWidgetRealize()
    {
        this.widget.disconnect(this._realizeSignal);
        this._realizeSignal = null;

        if(this.widget.get_error) {
            const error = this.widget.get_error();
            if(error) {
                debug('player widget error detected');
                debug(error);

                this.widget.add_css_class('blackbackground');
            }
        }

        const root = this.widget.get_root();
        if(!root) return;

        this.closeRequestSignal = root.connect(
            'close-request', this._onCloseRequest.bind(this)
        );
    }

    /* Widget only - does not happen when using controls navigation */
    _onWidgetKeyPressed(controller, keyval, keycode, state)
    {
        const clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        let bool = false;

        this.keyPressCount++;

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
                if(this.keyPressCount > 1)
                    clapperWidget.revealControls();
                break;
            default:
                break;
        }
    }

    /* Also happens after using controls navigation for selected keys */
    _onWidgetKeyReleased(controller, keyval, keycode, state)
    {
        const clapperWidget = this.widget.get_ancestor(Gtk.Grid);
        let value, root;

        this.keyPressCount = 0;

        switch(keyval) {
            case Gdk.KEY_space:
                this.toggle_play();
                break;
            case Gdk.KEY_Return:
                if(clapperWidget.isFullscreenMode)
                    clapperWidget.revealControls(true);
                break;
            case Gdk.KEY_Right:
            case Gdk.KEY_Left:
                value = Math.round(
                    clapperWidget.controls.positionScale.get_value()
                );
                this.seek_seconds(value);
                clapperWidget._setHideControlsTimeout();
                break;
            case Gdk.KEY_F11:
            case Gdk.KEY_f:
            case Gdk.KEY_F:
                clapperWidget.toggleFullscreen();
                break;
            case Gdk.KEY_q:
            case Gdk.KEY_Q:
                root = this.widget.get_root();
                root.emit('close-request');
                break;
            default:
                break;
        }
    }

    _onWindowMap(window)
    {
        this.windowMapped = true;
        this._playFirstTrack();
    }

    _onCloseRequest(window)
    {
        this._performCloseCleanup(window);

        if(this.state === GstClapper.ClapperState.STOPPED)
            return window.run_dispose();

        this.quitOnStop = true;
        this.stop();
    }

    _onSettingsKeyChanged(settings, key)
    {
        let root, value, action;

        switch(key) {
            case 'seeking-mode':
                this.seekingMode = settings.get_string('seeking-mode');
                switch(this.seekingMode) {
                    case 'fast':
                        this.set_seek_mode(GstClapper.ClapperSeekMode.FAST);
                        break;
                    case 'accurate':
                        this.set_seek_mode(GstClapper.ClapperSeekMode.ACCURATE);
                        break;
                    default:
                        this.set_seek_mode(GstClapper.ClapperSeekMode.DEFAULT);
                        break;
                }
                break;
            case 'render-shadows':
                root = this.widget.get_root();
                if(!root) break;

                const gpuClass = 'gpufriendly';
                const renderShadows = settings.get_boolean(key);
                const hasShadows = !root.has_css_class(gpuClass);

                if(renderShadows === hasShadows)
                    break;

                action = (renderShadows) ? 'remove' : 'add';
                root[action + '_css_class'](gpuClass);
                break;
            case 'audio-offset':
                value = Math.round(settings.get_double(key) * -Gst.MSECOND);
                this.set_audio_video_offset(value);
                debug(`set audio-video offset: ${value}`);
                break;
            case 'subtitle-offset':
                value = Math.round(settings.get_double(key) * -Gst.MSECOND);
                this.set_subtitle_video_offset(value);
                debug(`set subtitle-video offset: ${value}`);
                break;
            case 'dark-theme':
                root = this.widget.get_root();
                if(!root) break;

                root.application._onThemeChanged(Gtk.Settings.get_default());
                break;
            case 'play-flags':
                const initialFlags = this.pipeline.flags;
                const settingsFlags = settings.get_int(key);

                if(initialFlags === settingsFlags)
                    break;

                this.pipeline.flags = settingsFlags;
                debug(`changed play flags: ${initialFlags} -> ${settingsFlags}`);
                break;
            case 'webserver-enabled':
            case 'webapp-enabled':
                const webserverEnabled = settings.get_boolean('webserver-enabled');

                if(webserverEnabled) {
                    if(!WebServer) {
                        /* Probably most users will not use this,
                         * so conditional import for faster startup */
                        WebServer = imports.src.webServer.WebServer;
                    }

                    if(!this.webserver) {
                        this.webserver = new WebServer(settings.get_int('webserver-port'));
                        this.webserver.passMsgData = this.receiveWs.bind(this);
                    }
                    this.webserver.startListening();

                    const webappEnabled = settings.get_boolean('webapp-enabled');

                    if(!this.webapp && !webappEnabled)
                        break;

                    if(webappEnabled) {
                        if(!this.webapp)
                            this.webapp = new WebApp();

                        this.webapp.startDaemonApp(settings.get_int('webapp-port'));
                    }
                }
                else if(this.webserver) {
                    /* remote app will close when connection is lost
                     * which will cause the daemon to close too */
                    this.webserver.stopListening();
                }
                break;
            case 'webserver-port':
                if(!this.webserver)
                    break;

                this.webserver.setListeningPort(settings.get_int(key));
                break;
            default:
                break;
        }
    }
});
