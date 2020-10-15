const { Gdk, GLib, GObject, Gtk, Gst, GstPlayer, Pango } = imports.gi;
const { Controls } = imports.clapper_src.controls;
const Debug = imports.clapper_src.debug;
const Revealers = imports.clapper_src.revealers;

let { debug } = Debug;

var Interface = GObject.registerClass(
class ClapperInterface extends Gtk.Grid
{
    _init(opts)
    {
        Debug.gstVersionCheck();

        super._init();

        let defaults = {
            seekOnDrop: true
        };
        Object.assign(this, defaults, opts);

        this.fullscreenMode = false;
        this.lastVolumeValue = null;
        this.lastPositionValue = 0;
        this.lastRevealerEventTime = 0;
        this.needsTracksUpdate = true;
        this.headerBar = null;
        this.defaultTitle = null;

        this.videoBox = new Gtk.Box();
        this.overlay = new Gtk.Overlay();
        this.revealerTop = new Revealers.RevealerTop();
        this.revealerBottom = new Revealers.RevealerBottom();
        this.controls = new Controls();

        this.videoBox.add_css_class('videobox');
        this.videoBox.append(this.overlay);
        this.attach(this.videoBox, 0, 0, 1, 1);
        this.attach(this.controls, 0, 1, 1, 1);

        this.destroySignal = this.connect('destroy', this._onDestroy.bind(this));
    }

    addPlayer(player)
    {
        this._player = player;
        this._player.widget.vexpand = true;
        this._player.widget.hexpand = true;

        this._player.connect('state-changed', this._onPlayerStateChanged.bind(this));
        this._player.connect('volume-changed', this._onPlayerVolumeChanged.bind(this));
        this._player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));
        this._player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));

        this._player.scrollController.connect(
            'scroll', (ctl, dx, dy) => this.controls._onScroll(ctl, dx, dy)
        );
        this.controls.togglePlayButton.connect(
            'clicked', this._onControlsTogglePlayClicked.bind(this)
        );
        this.scaleSig = this.controls.positionScale.connect(
            'value-changed', this._onControlsPositionChanged.bind(this)
        );
        this.controls.volumeScale.connect(
            'value-changed', this._onControlsVolumeChanged.bind(this)
        );
        this.controls.connect(
            'position-seeking-changed', this._onPositionSeekingChanged.bind(this)
        );
        this.controls.connect(
            'track-change-requested', this._onTrackChangeRequested.bind(this)
        );
        this.controls.connect(
            'visualization-change-requested', this._onVisualizationChangeRequested.bind(this)
        );
        //this.revealerTop.connect('event-after', (self, event) => this._player.widget.event(event));

        this.overlay.set_child(this._player.widget);
        this.overlay.add_overlay(this.revealerTop);
        this.overlay.add_overlay(this.revealerBottom);

        this.overlay.show();
        this._player.widget.show();
    }

    addHeaderBar(headerBar, defaultTitle)
    {
        this.headerBar = headerBar;
        this.defaultTitle = defaultTitle || null;
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

    setFullscreenMode(isFullscreen)
    {
        if(this.fullscreenMode === isFullscreen)
            return;

        if(isFullscreen) {
            this.remove(this.controls);
            this.revealerBottom.append(this.controls);
        }
        else {
            this.revealerBottom.remove(this.controls);
            this.attach(this.controls, 0, 1, 1, 1);
        }

        this.controls.setFullscreenMode(isFullscreen);
        this.showControls(isFullscreen);

        this.fullscreenMode = isFullscreen;
        debug(`interface in fullscreen mode: ${isFullscreen}`);
    }

    updateMediaTracks()
    {
        let mediaInfo = this._player.get_media_info();

        /* Set titlebar media title and path */
        this.updateTitles(mediaInfo);

        // we can also check if video is "live" or "seekable" (right now unused)
        // it might be a good idea to hide position seek bar and disable seeking
        // when playing not seekable media (not implemented yet)
        //let isLive = mediaInfo.is_live();
        //let isSeekable = mediaInfo.is_seekable();

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
            let currStream = this._player[`get_current_${type}_track`]();
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
                if(this.controls[`${type}TracksButton`].visible) {
                    debug(`hiding popover button without contents: ${type}`);
                    this.controls[`${type}TracksButton`].hide();
                }
                continue;
            }
            this.controls.addCheckButtons(
                this.controls[`${type}TracksButton`].popoverBox,
                parsedInfo[`${type}Tracks`],
                activeId
            );
            if(!this.controls[`${type}TracksButton`].visible) {
                debug(`showing popover button with contents: ${type}`);
                this.controls[`${type}TracksButton`].show();
            }
        }
    }

    updateTitles(mediaInfo)
    {
        if(this.headerBar)
            this.headerBar.updateHeaderBar(mediaInfo);

        this.revealerTop.setMediaTitle(this.headerBar.titleLabel.label);
    }

    updateTime()
    {
        let currTime = GLib.DateTime.new_now_local();
        let endTime = currTime.add_seconds(
            this.controls.positionAdjustment.get_upper() - this.lastPositionValue
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

    _onTrackChangeRequested(self, type, activeId)
    {
        /* Reenabling audio is slow (as expected),
         * so it is better to toggle mute instead */
        if(type === 'audio') {
            if(activeId < 0)
                return this._player.set_mute(true);

            if(this._player.get_mute())
                this._player.set_mute(false);

            return this._player[`set_${type}_track`](activeId);
        }

        if(activeId < 0) {
            /* Disabling video leaves last frame frozen,
             * so we hide it by making it transparent */
            if(type === 'video')
                this._player.widget.set_opacity(0);

            return this._player[`set_${type}_track_enabled`](false);
        }

        this._player[`set_${type}_track`](activeId);
        this._player[`set_${type}_track_enabled`](true);

        if(type === 'video' && !this._player.widget.opacity) {
            this._player.widget.set_opacity(1);
            this._player.renderer.expose();
        }
    }

    _onVisualizationChangeRequested(self, visName)
    {
        let isEnabled = this._player.get_visualization_enabled();

        if(!visName) {
            if(isEnabled) {
                this._player.set_visualization_enabled(false);
                debug('disabled visualizations');
            }

            return;
        }

        let currVis = this._player.get_current_visualization();

        if(currVis === visName)
            return;

        debug(`set visualization: ${visName}`);
        this._player.set_visualization(visName);

        if(!isEnabled) {
            this._player.set_visualization_enabled(true);
            debug('enabled visualizations');
        }
    }

    _onPlayerStateChanged(player, state)
    {
        switch(state) {
            case GstPlayer.PlayerState.BUFFERING:
                break;
            case GstPlayer.PlayerState.STOPPED:
                this.needsTracksUpdate = true;
            case GstPlayer.PlayerState.PAUSED:
                this.controls.togglePlayButton.setPrimaryIcon();
                break;
            case GstPlayer.PlayerState.PLAYING:
                this.controls.togglePlayButton.setSecondaryIcon();
                if(this.needsTracksUpdate) {
                    this.needsTracksUpdate = false;
                    this.updateMediaTracks();
                }
                break;
            default:
                break;
        }
    }

    _onPlayerDurationChanged(player)
    {
        let duration = player.get_duration() / 1000000000;
        let increment = (duration < 1)
            ? 0
            : (duration < 100)
            ? 1
            : duration / 100;

        this.controls.positionAdjustment.set_upper(Math.floor(duration));
        this.controls.positionAdjustment.set_step_increment(increment);
        this.controls.positionAdjustment.set_page_increment(increment);

        this.controls.durationFormated = this.controls._getFormatedTime(duration);
        this.controls._onPositionScaleValueChanged();
    }

    _onPlayerPositionUpdated(player, position)
    {
        if(
            this.controls.isPositionSeeking
            || this._player.state === GstPlayer.PlayerState.BUFFERING
        )
            return;

        let positionSeconds = Math.round(position / 1000000000);

        if(positionSeconds === this.lastPositionValue)
            return;

        this.lastPositionValue = positionSeconds;
        this.controls.positionScale.set_value(positionSeconds);
    }

    _onPlayerVolumeChanged()
    {
        let volume = Number(this._player.get_volume().toFixed(2));

        if(volume === this.lastVolumeValue)
            return;

        this.lastVolumeValue = volume;
        this.controls.volumeScale.set_value(volume);
    }

    _onPositionSeekingChanged(self, isPositionSeeking)
    {
        if(isPositionSeeking || !this.seekOnDrop)
            return;

        this._onControlsPositionChanged(this.controls.positionScale);
    }

    _onControlsTogglePlayClicked()
    {
        this._player.toggle_play();
    }

    _onControlsPositionChanged(positionScale)
    {
        if(this.seekOnDrop && this.controls.isPositionSeeking)
            return;

        let positionSeconds = Math.round(positionScale.get_value());

        if(positionSeconds === this.lastPositionValue)
            return;

        this.lastPositionValue = positionSeconds;
        this._player.seek_seconds(positionSeconds);

        if(this.fullscreenMode)
            this.updateTime();
    }

    _onControlsVolumeChanged(volumeScale)
    {
        let volume = Number(volumeScale.get_value().toFixed(2));

        let icon = (volume <= 0)
            ? 'muted'
            : (volume <= 0.33)
            ? 'low'
            : (volume <= 0.66)
            ? 'medium'
            : (volume <= 1)
            ? 'high'
            : 'overamplified';

        let iconName = `audio-volume-${icon}-symbolic`;
        if(this.controls.volumeButton.icon_name !== iconName)
        {
            debug(`set volume icon: ${icon}`);
            this.controls.volumeButton.set_icon_name(iconName);
        }

        if(volume === this.lastVolumeValue)
            return;

        this.lastVolumeValue = volume;
        this._player.set_volume(volume);
    }

    _onDestroy()
    {
        this.disconnect(this.destroySignal);

        if(
            this._player
            && this._player.state !== GstPlayer.PlayerState.STOPPED
        )
            this._player.stop();

        this.controls.emit('destroy');
    }
});
