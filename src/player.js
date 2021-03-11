const { Gdk, Gio, GObject, Gst, GstClapper, Gtk } = imports.gi;
const ByteArray = imports.byteArray;
const Dash = imports.src.dash;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const YouTube = imports.src.youtube;
const { PlayerBase } = imports.src.playerBase;

const { debug } = Debug;
const { settings } = Misc;

var Player = GObject.registerClass(
class ClapperPlayer extends PlayerBase
{
    _init()
    {
        super._init();

        this.seek_done = true;
        this.doneStartup = false;
        this.needsFastSeekRestore = false;
        this.customVideoTitle = null;

        this.playOnFullscreen = false;
        this.quitOnStop = false;
        this.needsTocUpdate = true;

        this.keyPressCount = 0;

        const keyController = new Gtk.EventControllerKey();
        keyController.connect('key-pressed', this._onWidgetKeyPressed.bind(this));
        keyController.connect('key-released', this._onWidgetKeyReleased.bind(this));
        this.widget.add_controller(keyController);

        this.connect('state-changed', this._onStateChanged.bind(this));
        this.connect('uri-loaded', this._onUriLoaded.bind(this));
        this.connect('end-of-stream', this._onStreamEnded.bind(this));
        this.connect('warning', this._onPlayerWarning.bind(this));
        this.connect('error', this._onPlayerError.bind(this));

        this._realizeSignal = this.widget.connect('realize', this._onWidgetRealize.bind(this));
    }

    set_uri(uri)
    {
        this.customVideoTitle = null;

        if(Gst.Uri.get_protocol(uri) !== 'file') {
            const [isYouTubeUri, videoId] = YouTube.checkYouTubeUri(uri);

            if(!isYouTubeUri)
                return super.set_uri(uri);

            this.getYouTubeUriAsync(videoId)
                .then(ytUri => super.set_uri(ytUri))
                .catch(debug);

            return;
        }

        let file = Gio.file_new_for_uri(uri);
        if(!file.query_exists(null)) {
            debug(`file does not exist: ${file.get_path()}`, 'LEVEL_WARNING');

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

    async getYouTubeUriAsync(videoId)
    {
        const client = new YouTube.YouTubeClient();
        const info = await client.getVideoInfoPromise(videoId).catch(debug);

        if(!info)
            throw new Error('no YouTube video info');

        const dash = Dash.generateDash(info);
        const videoUri = (dash)
            ? await Dash.saveDashPromise(dash).catch(debug)
            : client.getBestCombinedUri(info);

        if(!videoUri)
            throw new Error('no YouTube video URI');

        this.customVideoTitle = (info.videoDetails && info.videoDetails.title)
            ? info.videoDetails.title
            : videoId;

        return videoUri;
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

    _preparePlaylist(playlist)
    {
        this.playlistWidget.removeAll();

        for(let source of playlist) {
            const uri = (source.get_uri != null)
                ? source.get_uri()
                : Gst.uri_is_valid(source)
                ? source
                : Gst.filename_to_uri(source);

            this.playlistWidget.addItem(uri);
        }
    }

    set_playlist(playlist)
    {
        this._preparePlaylist(playlist);

        const firstTrack = this.playlistWidget.get_row_at_index(0);
        if(!firstTrack) return;

        firstTrack.activate();
    }

    set_subtitles(source)
    {
        const uri = (source.get_uri)
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
        this.seek(seconds * 1000000000);
    }

    seek_chapter(seconds)
    {
        if(this.seekingMode !== 'fast') {
            this.seek_seconds(seconds);
            return;
        }

        /* FIXME: Remove this check when GstPlay(er) have set_seek_mode function */
        if(this.set_seek_mode) {
            this.set_seek_mode(GstClapper.ClapperSeekMode.DEFAULT);
            this.seekingMode = 'normal';
            this.needsFastSeekRestore = true;
        }

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

    receiveWs(action, value)
    {
        switch(action) {
            case 'toggle_play':
            case 'play':
            case 'pause':
            case 'set_playlist':
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
                        super.receiveWs(action, value);
                        break;
                }
                break;
        }
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
            let resumeInfo = {};
            if(settings.get_boolean('resume-enabled')) {
                const resumeTitle = this.playlistWidget.getActiveFilename();
                const resumeTime = Math.floor(this.position / 1000000000);
                const resumeDuration = this.duration / 1000000000;

                /* Do not save resume info when title is too long (random URI),
                 * video is very short, just started or almost finished */
                if(
                    resumeTitle.length < 300
                    && resumeDuration > 60
                    && resumeTime > 15
                    && resumeDuration - resumeTime > 20
                ) {
                    resumeInfo.title = resumeTitle;
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

        if(!this.doneStartup) {
            this.doneStartup = true;

            if(settings.get_boolean('fullscreen-auto')) {
                const root = player.widget.get_root();
                const clapperWidget = root.get_child();
                if(!clapperWidget.isFullscreenMode) {
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
            case Gdk.KEY_Escape:
                if(clapperWidget.isFullscreenMode) {
                    root = this.widget.get_root();
                    root.unfullscreen();
                }
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

    _onCloseRequest(window)
    {
        this._performCloseCleanup(window);

        if(this.state === GstClapper.ClapperState.STOPPED)
            return window.run_dispose();

        this.quitOnStop = true;
        this.stop();
    }
});
