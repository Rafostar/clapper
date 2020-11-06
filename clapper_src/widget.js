const { Gdk, GLib, GObject, Gst, GstPlayer, Gtk } = imports.gi;
const { Controls } = imports.clapper_src.controls;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const { Player } = imports.clapper_src.player;
const Revealers = imports.clapper_src.revealers;

let { debug } = Debug;

var Widget = GObject.registerClass({
    Signals: {
        'fullscreen-changed': {
            param_types: [GObject.TYPE_BOOLEAN]
        }
    }
}, class ClapperWidget extends Gtk.Grid
{
    _init(opts)
    {
        Debug.gstVersionCheck();

        super._init();

        let clapperPath = Misc.getClapperPath();
        let defaults = {
            cssPath: `${clapperPath}/css/styles.css`,
        };
        Object.assign(this, defaults, opts);

        let cssProvider = new Gtk.CssProvider();
        cssProvider.load_from_path(this.cssPath);
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            cssProvider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        this.fullscreenMode = false;
        this.floatingMode = false;
        this.isSeekable = false;

        this.lastRevealerEventTime = 0;
        this.needsTracksUpdate = true;
        this.mediaInfoSignal = null;

        this.videoBox = new Gtk.Box();
        this.overlay = new Gtk.Overlay();
        this.revealerTop = new Revealers.RevealerTop();
        this.revealerBottom = new Revealers.RevealerBottom();
        this.controls = new Controls();

        this.videoBox.add_css_class('videobox');
        this.videoBox.append(this.overlay);
        this.attach(this.videoBox, 0, 0, 1, 1);
        this.attach(this.controls, 0, 1, 1, 1);

        this.mapSignal = this.connect('map', this._onMap.bind(this));

        this.player = new Player();
        this.player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));
        this.player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));
        this.player.connect('volume-changed', this._onPlayerVolumeChanged.bind(this));

        this.overlay.set_child(this.player.widget);
        this.overlay.add_overlay(this.revealerTop);
        this.overlay.add_overlay(this.revealerBottom);

        let motionController = new Gtk.EventControllerMotion();
        motionController.connect('leave', this._onLeave.bind(this));
        this.add_controller(motionController);
    }

    revealControls(isReveal)
    {
        for(let pos of ['Top', 'Bottom'])
            this[`revealer${pos}`].revealChild(isReveal);
    }

    showControls(isShow)
    {
        for(let pos of ['Top', 'Bottom'])
            this[`revealer${pos}`].showChild(isShow);
    }

    toggleFullscreen()
    {
        let root = this.get_root();
        if(!root) return;

        let un = (this.fullscreenMode) ? 'un' : '';
        root[`${un}fullscreen`]();
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.fullscreenMode === isFullscreen)
            return;

        this.fullscreenMode = isFullscreen;

        if(!this.floatingMode)
            this._changeControlsPlacement(isFullscreen);
        else {
            this._setWindowFloating(!isFullscreen);
            this.revealerBottom.setFloatingClass(!isFullscreen);
            this.controls.setFloatingMode(!isFullscreen);
            this.controls.unfloatButton.set_visible(!isFullscreen);
        }

        this.controls.setFullscreenMode(isFullscreen);
        this.showControls(isFullscreen);
        this.player.widget.grab_focus();

        if(this.player.playOnFullscreen && isFullscreen) {
            this.player.playOnFullscreen = false;
            this.player.play();
        }
    }

    setFloatingMode(isFloating)
    {
        if(this.floatingMode === isFloating)
            return;

        this.floatingMode = isFloating;

        this.revealerBottom.setFloatingClass(isFloating);
        this._changeControlsPlacement(isFloating);
        this.controls.setFloatingMode(isFloating);
        this.controls.unfloatButton.set_visible(isFloating);
        this.revealerBottom.showChild(isFloating);
        this._setWindowFloating(isFloating);

        this.player.widget.grab_focus();
    }

    _setWindowFloating(isFloating)
    {
        let root = this.get_root();

        let cssClass = 'floatingwindow';
        if(isFloating === root.has_css_class(cssClass))
            return;

        let action = (isFloating) ? 'add' : 'remove';
        root[action + '_css_class'](cssClass);
    }

    _changeControlsPlacement(isOnTop)
    {
        if(isOnTop) {
            this.remove(this.controls);
            this.revealerBottom.append(this.controls);
        }
        else {
            this.revealerBottom.remove(this.controls);
            this.attach(this.controls, 0, 1, 1, 1);
        }
    }

    _onMediaInfoUpdated(player, mediaInfo)
    {
        player.disconnect(this.mediaInfoSignal);

        /* Set titlebar media title and path */
        this.updateTitles(mediaInfo);

        /* Show/hide position scale on LIVE */
        let isLive = mediaInfo.is_live();
        this.isSeekable = mediaInfo.is_seekable();
        this.controls.setLiveMode(isLive, this.isSeekable);

        let streamList = mediaInfo.get_stream_list();
        let parsedInfo = {
            videoTracks: [],
            audioTracks: [],
            subtitleTracks: []
        };

        for(let info of streamList) {
            let type, text;

            switch(info.constructor) {
                case GstPlayer.PlayerVideoInfo:
                    type = 'video';
                    text = info.get_codec() + ', ' +
                        + info.get_width() + 'x'
                        + info.get_height();
                    let fps = info.get_framerate();
                    fps = Number((fps[0] / fps[1]).toFixed(2));
                    if(fps)
                        text += `@${fps}`;
                    break;
                case GstPlayer.PlayerAudioInfo:
                    type = 'audio';
                    let codec = info.get_codec();
                    if(codec.includes('(')) {
                        codec = codec.substring(
                            codec.indexOf('(') + 1,
                            codec.indexOf(')')
                        );
                    }
                    text = info.get_language() || 'Unknown';
                    text += ', ' + codec + ', '
                        + info.get_channels() + ' Channels';
                    break;
                case GstPlayer.PlayerSubtitleInfo:
                    type = 'subtitle';
                    text = info.get_language() || 'Unknown';
                    break;
                default:
                    debug(`unrecognized media info type: ${info.constructor}`);
                    break;
            }
            let tracksArr = parsedInfo[`${type}Tracks`];
            if(!tracksArr.length)
            {
                tracksArr[0] = {
                    label: 'Disabled',
                    type: type,
                    activeId: -1
                };
            }
            tracksArr.push({
                label: text,
                type: type,
                activeId: info.get_index(),
            });
        }

        for(let type of ['video', 'audio', 'subtitle']) {
            let currStream = player[`get_current_${type}_track`]();
            let activeId = (currStream) ? currStream.get_index() : -1;

            if(currStream && type !== 'subtitle') {
                let caps = currStream.get_caps();
                debug(`${type} caps: ${caps.to_string()}`, 'LEVEL_INFO');
            }
            if(type === 'video') {
                let isShowVis = (parsedInfo[`${type}Tracks`].length === 0);
                this.showVisualizationsButton(isShowVis);
            }
            if(!parsedInfo[`${type}Tracks`].length) {
                debug(`hiding popover button without contents: ${type}`);
                this.controls[`${type}TracksButton`].set_visible(false);

                continue;
            }
            this.controls.addCheckButtons(
                this.controls[`${type}TracksButton`].popoverBox,
                parsedInfo[`${type}Tracks`],
                activeId
            );
            debug(`showing popover button with contents: ${type}`);
            this.controls[`${type}TracksButton`].set_visible(true);
        }

        this.mediaInfoSignal = null;
    }

    updateTitles(mediaInfo)
    {
        let title = mediaInfo.get_title();
        let subtitle = mediaInfo.get_uri();

        if(Gst.Uri.get_protocol(subtitle) === 'file') {
            subtitle = GLib.path_get_basename(
                GLib.filename_from_uri(subtitle)[0]
            );
        }
        if(!title) {
            title = (!subtitle)
                ? this.defaultTitle
                : (subtitle.includes('.'))
                ? subtitle.split('.').slice(0, -1).join('.')
                : subtitle;

            subtitle = null;
        }

        let root = this.get_root();
        let headerbar = root.get_titlebar();

        if(headerbar && headerbar.updateHeaderBar)
            headerbar.updateHeaderBar(title, subtitle);

        this.revealerTop.setMediaTitle(title);
    }

    updateTime()
    {
        if(!this.revealerTop.visible)
            return null;

        let currTime = GLib.DateTime.new_now_local();
        let endTime = currTime.add_seconds(
            this.controls.positionAdjustment.get_upper() - this.controls.currentPosition
        );
        let nextUpdate = this.revealerTop.setTimes(currTime, endTime);

        return nextUpdate;
    }

    showVisualizationsButton(isShow)
    {
        if(isShow && !this.controls.visualizationsButton.isVisList) {
            debug('creating visualizations list');
            let visArr = GstPlayer.Player.visualizations_get();
            if(!visArr.length)
                return;

            let parsedVisArr = [{
                label: 'Disabled',
                type: 'visualization',
                activeId: null
            }];

            visArr.forEach(vis => {
                parsedVisArr.push({
                    label: vis.name[0].toUpperCase() + vis.name.substring(1),
                    type: 'visualization',
                    activeId: vis.name,
                });
            });

            this.controls.addCheckButtons(
                this.controls.visualizationsButton.popoverBox,
                parsedVisArr,
                null
            );
            this.controls.visualizationsButton.isVisList = true;
            debug(`total visualizations: ${visArr.length}`);
        }

        if(this.controls.visualizationsButton.visible === isShow)
            return;

        let action = (isShow) ? 'show' : 'hide';
        this.controls.visualizationsButton[action]();
        debug(`show visualizations button: ${isShow}`);
    }

    _onPlayerStateChanged(player, state)
    {
        switch(state) {
            case GstPlayer.PlayerState.BUFFERING:
                if(!player.is_local_file) {
                    this.needsTracksUpdate = true;
                }
                debug('player state changed to: BUFFERING');
                break;
            case GstPlayer.PlayerState.STOPPED:
                this.controls.currentPosition = 0;
                this.controls.positionScale.set_value(0);
                this.controls.togglePlayButton.setPrimaryIcon();
                this.needsTracksUpdate = true;
                if(this.mediaInfoSignal) {
                    player.disconnect(this.mediaInfoSignal);
                    this.mediaInfoSignal = null;
                }
                this.controls.togglePlayButton.setPrimaryIcon();
                debug('player state changed to: STOPPED');
                break;
            case GstPlayer.PlayerState.PAUSED:
                this.controls.togglePlayButton.setPrimaryIcon();
                debug('player state changed to: PAUSED');
                break;
            case GstPlayer.PlayerState.PLAYING:
                this.controls.togglePlayButton.setSecondaryIcon();
                if(this.needsTracksUpdate && !this.mediaInfoSignal) {
                    this.needsTracksUpdate = false;
                    this.mediaInfoSignal = player.connect(
                        'media_info_updated', this._onMediaInfoUpdated.bind(this)
                    );
                }
                debug('player state changed to: PLAYING');
                break;
            default:
                break;
        }

        let isNotStopped = (state !== GstPlayer.PlayerState.STOPPED)
        this.revealerTop.endTime.set_visible(isNotStopped);

        if(state === GstPlayer.PlayerState.BUFFERING)
            return;

        let window = this.get_root();
        Misc.inhibitForState(state, window);
    }

    _onPlayerDurationChanged(player)
    {
        let duration = Math.floor(player.get_duration() / 1000000000);

        /* Sometimes GstPlayer might re-emit
         * duration changed during playback */
        if(this.controls.currentDuration === duration)
            return;

        this.controls.currentDuration = duration;
        this.controls.positionAdjustment.set_upper(duration);
        this.controls.durationFormated = Misc.getFormatedTime(duration);
        this.controls.updateElapsedLabel();
    }

    _onPlayerPositionUpdated(player, position)
    {
        if(
            !this.isSeekable
            || this.controls.isPositionDragging
            || !player.seek_done
        )
            return;

        let positionSeconds = Math.round(position / 1000000000);
        if(positionSeconds === this.controls.currentPosition)
            return;

        this.controls.positionScale.set_value(positionSeconds);
    }

    _onPlayerVolumeChanged(player)
    {
        let volume = Number(player.get_volume().toFixed(2));
        if(volume === this.currentVolume)
            return;

        this.controls.currentVolume = volume;
        this.controls.volumeScale.set_value(volume);
    }

    _onStateNotify(toplevel)
    {
        let isFullscreen = Boolean(
            toplevel.state & Gdk.ToplevelState.FULLSCREEN
        );

        if(this.fullscreenMode === isFullscreen)
            return;

        this.setFullscreenMode(isFullscreen);
        this.emit('fullscreen-changed', isFullscreen);
        debug(`interface in fullscreen mode: ${isFullscreen}`);
    }

    _onLeave(controller)
    {
        if(
            this.fullscreenMode
            || !this.floatingMode
            || this.player.isWidgetDragging
        )
            return;

        this.revealerBottom.revealChild(false);
    }

    _onMap()
    {
        this.disconnect(this.mapSignal);

        let root = this.get_root();
        if(!root) return;

        let surface = root.get_surface();
        surface.connect('notify::state', this._onStateNotify.bind(this));
    }
});
