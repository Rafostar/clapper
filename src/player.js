const { Adw, Gdk, Gio, GLib, GObject, Gst, GstClapper, Gtk } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const { PlaylistWidget } = imports.src.playlist;

const { debug, warn } = Debug;
const { settings } = Misc;

let WebServer;

var Player = GObject.registerClass({
    GTypeName: 'ClapperPlayer',
},
class ClapperPlayer extends GstClapper.Clapper
{
    _init()
    {
        let vsink = null;
        const use_legacy_sink = GLib.getenv('CLAPPER_USE_LEGACY_SINK');

        if(!use_legacy_sink || use_legacy_sink != '1') {
            vsink = Gst.ElementFactory.make('clappersink', null);
            this.clappersink = vsink;
        }

        if(!vsink) {
            vsink = Gst.ElementFactory.make('glsinkbin', null);
            const gtk4plugin = new GstClapper.ClapperGtk4Plugin();

            warn('using legacy video sink');

            this.clappersink = gtk4plugin.video_sink;
            vsink.sink = this.clappersink;
        }

        super._init({
            signal_dispatcher: new GstClapper.ClapperGMainContextSignalDispatcher(),
            video_renderer: new GstClapper.ClapperVideoOverlayVideoRenderer({
                video_sink: vsink,
            }),
            mpris: new GstClapper.ClapperMpris({
                own_name: `org.mpris.MediaPlayer2.${Misc.appName}`,
                id_path: '/' + Misc.appId.replace(/\./g, '/'),
                identity: Misc.appName,
                desktop_entry: Misc.appId,
                default_art_url: Misc.getClapperThemeIconUri(),
            }),
            use_playbin3: settings.get_boolean('use-playbin3'),
            use_pipewire: settings.get_boolean('use-pipewire'),
        });

        this.widget = this.clappersink.widget;
        this.widget.add_css_class('videowidget');

        this.visualization_enabled = false;

        this.webserver = null;
        this.playlistWidget = new PlaylistWidget();

        this.seekDone = true;
        this.needsFastSeekRestore = false;

        this.windowMapped = false;
        this.quitOnStop = false;
        this.needsTocUpdate = true;

        this.set_all_plugins_ranks();
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
            'dark-theme',
            'after-playback',
            'seeking-mode',
            'audio-offset',
            'play-flags',
            'webserver-enabled'
        ];

        for(let key of settingsToSet)
            this._onSettingsKeyChanged(settings, key);

        const flag = Gio.SettingsBindFlags.GET;
        settings.bind('subtitle-font', this.pipeline, 'subtitle-font-desc', flag);
    }

    set_all_plugins_ranks()
    {
        let data = {};
        let hadErr = false;

        /* Set empty plugin list if someone messed it externally */
        try {
            data = JSON.parse(settings.get_string('plugin-ranking'));
            if(Array.isArray(data)) {
                data = {};
                hadErr = true;
            }
        }
        catch(err) {
            debug(err);
            hadErr = true;
        }

        if(hadErr) {
            settings.set_string('plugin-ranking', "{}");
            debug('restored plugin ranking to defaults');
        }

        for(let plugin of Object.keys(data))
            this.set_plugin_rank(plugin, data[plugin]);
    }

    set_plugin_rank(name, rank)
    {
        const gstRegistry = Gst.Registry.get();
        const feature = gstRegistry.lookup_feature(name);
        if(!feature) {
            warn(`cannot change rank of unavailable plugin: ${name}`);
            return;
        }

        const oldRank = feature.get_rank();
        if(rank === oldRank)
            return;

        feature.set_rank(rank);
        debug(`changed rank: ${oldRank} -> ${rank} for ${name}`);
    }

    set_uri(uri)
    {
        if(Misc.getUriProtocol(uri) === 'file') {
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

        debug('new playlist');
        this.playlistWidget.removeAll();
        this._addPlaylistItems(playlist);

        if(settings.get_boolean('fullscreen-auto')) {
            const { root } = this.playlistWidget;
            /* Do not enter fullscreen when already in it
             * or when in floating mode */
            if(
                root
                && root.child
                && !root.child.isFullscreenMode
                && root.child.controlsRevealer.reveal_child
            )
                root.fullscreen();
        }

        /* If not mapped yet, first track will play after map */
        if(this.windowMapped)
            this._playFirstTrack();
    }

    append_playlist(playlist)
    {
        debug('appending playlist');
        this._addPlaylistItems(playlist);

        if(
            !this.windowMapped
            || this.state !== GstClapper.ClapperState.STOPPED
        )
            return;

        if(!this.playlistWidget.nextTrack())
            debug('playlist append failed');
    }

    set_subtitles(source)
    {
        const uri = this._getSourceUri(source);

        /* Check local file existence */
        if(
            Misc.getUriProtocol(uri) === 'file'
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

        this.seekDone = false;

        if(this.state === GstClapper.ClapperState.STOPPED)
            this.pause();

        if(position < 0)
            position = 0;

        super.seek(position);
    }

    seek_seconds(seconds)
    {
        this.seek(seconds * Gst.SECOND);
    }

    seek_chapter(seconds)
    {
        if(this.seek_mode !== GstClapper.ClapperSeekMode.FAST) {
            this.seek_seconds(seconds);
            return;
        }

        this.set_seek_mode(GstClapper.ClapperSeekMode.DEFAULT);
        this.needsFastSeekRestore = true;

        this.seek_seconds(seconds);
    }

    adjust_position(isIncrease)
    {
        this.seekDone = false;

        const { controls } = this.widget.get_ancestor(Gtk.Grid);
        const max = controls.positionAdjustment.get_upper();

        let seekingValue = settings.get_int('seeking-value');

        switch(settings.get_int('seeking-unit')) {
            case 2: /* Percentage */
                seekingValue *= max / 100;
                break;
            case 1: /* Minute */
                seekingValue *= 60;
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
        const scale = controls.volumeButton.volumeScale;

        scale.set_value(scale.get_value() + value);
    }

    next_chapter()
    {
        return this._switchChapter(false);
    }

    prev_chapter()
    {
        return this._switchChapter(true);
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
                this[action]();
                break;
            case 'seek':
            case 'set_playlist':
            case 'append_playlist':
            case 'set_subtitles':
                this[action](value);
                break;
            case 'change_playlist_item':
                this.playlistWidget.changeActiveRow(value);
                break;
            case 'toggle_fullscreen':
            case 'volume_up':
            case 'volume_down':
            case 'next_track':
            case 'prev_track':
            case 'next_chapter':
            case 'prev_chapter':
                this.widget.activate_action(`app.${action}`, null);
                break;
            case 'toggle_maximized':
                action = 'toggle-maximized';
            case 'minimize':
            case 'close':
                this.widget.activate_action(`window.${action}`, null);
                break;
            default:
                warn(`unhandled WebSocket action: ${action}`);
                break;
        }
    }

    _switchChapter(isPrevious)
    {
        if(this.state === GstClapper.ClapperState.STOPPED)
            return false;

        const { chapters } = this.widget.root.child.controls;
        if(!chapters)
            return false;

        const now = this.position / Gst.SECOND;
        const chapterTimes = Object.keys(chapters).sort((a, b) => a - b);
        if(isPrevious)
            chapterTimes.reverse();

        const chapter = chapterTimes.find(time => (isPrevious)
            ? now - 2.5 > time
            : now < time
        );
        if(!chapter)
            return false;

        this.seek_chapter(chapter);

        return true;
    }

    _addPlaylistItems(playlist)
    {
        for(let source of playlist) {
            const uri = this._getSourceUri(source);

            debug(`added uri: ${uri}`);
            this.playlistWidget.addItem(uri);
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

        if(!this.seekDone && state !== GstClapper.ClapperState.BUFFERING) {
            clapperWidget.updateTime();

            if(this.needsFastSeekRestore) {
                this.set_seek_mode(GstClapper.ClapperSeekMode.FAST);
                this.needsFastSeekRestore = false;
            }

            this.seekDone = true;
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

        if(this.playlistWidget._handleStreamEnded(player))
            return;

        /* After playback equal 2 means close the app */
        if(settings.get_int('after-playback') === 2) {
            /* Stop will be automatically called soon afterwards */
            this.quitOnStop = true;
            this._performCloseCleanup(this.widget.get_root());
        }

        /* When this signal is connected player
         * wants us to decide if it should stop */
        this.stop();
    }

    _onUriLoaded(player, uri)
    {
        debug(`URI loaded: ${uri}`);
        this.needsTocUpdate = true;

        player.play();
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
            case 'after-playback':
                this.clappersink.keep_last_frame = (settings.get_int(key) === 1);
                break;
            case 'seeking-mode':
                switch(settings.get_int(key)) {
                    case 2: /* Fast */
                        this.set_seek_mode(GstClapper.ClapperSeekMode.FAST);
                        break;
                    case 1: /* Accurate */
                        this.set_seek_mode(GstClapper.ClapperSeekMode.ACCURATE);
                        break;
                    default: /* Normal */
                        this.set_seek_mode(GstClapper.ClapperSeekMode.DEFAULT);
                        break;
                }
                break;
            case 'dark-theme':
                /* TODO: Remove libadwaita alpha2 compat someday */
                if (Adw.StyleManager != null) {
                    const styleManager = Adw.StyleManager.get_default();
                    styleManager.color_scheme = (settings.get_boolean(key))
                        ? Adw.ColorScheme.FORCE_DARK
                        : Adw.ColorScheme.FORCE_LIGHT;
                }
                else {
                    const gtkSettings = Gtk.Settings.get_default();
                    gtkSettings.gtk_application_prefer_dark_theme = settings.get_boolean(key);
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
                value = Math.round(settings.get_int(key) * -Gst.MSECOND);
                this.set_audio_video_offset(value);
                debug(`set audio-video offset: ${value}`);
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
                }
                else if(this.webserver) {
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
