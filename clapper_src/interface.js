const { Gdk, GLib, GObject, Gtk, Gst, GstPlayer, Pango } = imports.gi;
const { Controls } = imports.clapper_src.controls;
const Debug = imports.clapper_src.debug;

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

        this.controlsInVideo = false;
        this.lastVolumeValue = null;
        this.lastPositionValue = 0;
        this.needsTracksUpdate = true;
        this.revealTime = 800;
        this.headerBar = null;
        this.defaultTitle = null;

        let initTime = GLib.DateTime.new_now_local().format('%X');
        this.timeFormat = (initTime.length > 8)
            ? '%I:%M %p'
            : '%H:%M';

        this.videoBox = new Gtk.Box();
        this.overlay = new Gtk.Overlay();
        this.revealerTop = new Gtk.Revealer({
            transition_duration: this.revealTime,
            transition_type: Gtk.RevealerTransitionType.CROSSFADE,
        });
        this.revealerBottom = new Gtk.Revealer({
            transition_duration: this.revealTime,
            transition_type: Gtk.RevealerTransitionType.SLIDE_UP,
            valign: Gtk.Align.END,
        });
        this.revealerGridTop = new Gtk.Grid({
            column_spacing: 8
        });
        this.revealerBoxBottom = new Gtk.Box();
        this.controls = new Controls();

        this.fsTitle = new Gtk.Label({
            ellipsize: Pango.EllipsizeMode.END,
            expand: true,
            margin_top: 14,
            margin_left: 12,
            xalign: 0,
            yalign: 0,
        });

        let timeLabelOpts = {
            margin_right: 10,
            xalign: 1,
            yalign: 0,
        };
        this.fsTime = new Gtk.Label(timeLabelOpts);
        this.fsEndTime = new Gtk.Label(timeLabelOpts);

        this.revealerTop.set_events(
            Gdk.EventMask.BUTTON_PRESS_MASK
            | Gdk.EventMask.BUTTON_RELEASE_MASK
            | Gdk.EventMask.TOUCH_MASK
            | Gdk.EventMask.SCROLL_MASK
            | Gdk.EventMask.TOUCHPAD_GESTURE_MASK
            | Gdk.EventMask.POINTER_MOTION_MASK
            | Gdk.EventMask.ENTER_NOTIFY_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        );

        this.revealerGridTop.attach(this.fsTitle, 0, 0, 1, 1);
        this.revealerGridTop.attach(this.fsTime, 1, 0, 1, 1);
        this.revealerGridTop.attach(this.fsEndTime, 1, 0, 1, 1);

        this.videoBox.get_style_context().add_class('videobox');
        let revealerGridTopContext = this.revealerGridTop.get_style_context();
        revealerGridTopContext.add_class('osd');
        revealerGridTopContext.add_class('reavealertop');
        this.revealerBoxBottom.get_style_context().add_class('osd');

        this.fsTime.get_style_context().add_class('osdtime');
        this.fsEndTime.get_style_context().add_class('osdendtime');

        this.videoBox.pack_start(this.overlay, true, true, 0);
        this.revealerBottom.add(this.revealerBoxBottom);
        this.revealerTop.add(this.revealerGridTop);
        this.attach(this.videoBox, 0, 0, 1, 1);
        this.attach(this.controls, 0, 1, 1, 1);

        this.revealerTop.add_events(Gdk.EventMask.BUTTON_PRESS_MASK);
        this.revealerTop.show_all();
        this.revealerBottom.show_all();
    }

    addPlayer(player)
    {
        this._player = player;
        this._player.widget.expand = true;

        this._player.connect('state-changed', this._onPlayerStateChanged.bind(this));
        this._player.connect('volume-changed', this._onPlayerVolumeChanged.bind(this));
        this._player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));
        this._player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));

        this.controls.togglePlayButton.connect(
            'clicked', this._onControlsTogglePlayClicked.bind(this)
        );
        this.controls.positionScale.connect(
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

        this.overlay.add(this._player.widget);
        this.overlay.add_overlay(this.revealerTop);
        this.overlay.add_overlay(this.revealerBottom);

        this.revealerTop.connect(
            'scroll-event', (self, event) => this.controls._onScrollEvent(event)
        );
    }

    addHeaderBar(headerBar, defaultTitle)
    {
        this.headerBar = headerBar;
        this.defaultTitle = defaultTitle || null;
    }

    revealControls(isReveal)
    {
        for(let pos of ['Bottom', 'Top']) {
            this[`revealer${pos}`].set_transition_duration(this.revealTime);
            this[`revealer${pos}`].set_reveal_child(isReveal);
        }
    }

    showControls(isShow)
    {
        for(let pos of ['Bottom', 'Top']) {
            this[`revealer${pos}`].set_transition_duration(0);
            this[`revealer${pos}`].set_reveal_child(isShow);
        }
    }

    setControlsOnVideo(isOnVideo)
    {
        if(this.controlsInVideo === isOnVideo)
            return;

        if(isOnVideo) {
            this.remove(this.controls);
            this.controls.pack_start(this.controls.unfullscreenButton.box, false, false, 0);
            this.revealerBoxBottom.pack_start(this.controls, false, true, 0);
        }
        else {
            this.revealerBoxBottom.remove(this.controls);
            this.controls.remove(this.controls.unfullscreenButton.box);
            this.attach(this.controls, 0, 1, 1, 1);
        }

        this.controlsInVideo = isOnVideo;
        debug(`placed controls in overlay: ${isOnVideo}`);
    }

    updateMediaTracks()
    {
        let mediaInfo = this._player.get_media_info();

        // set titlebar media title and path
        this.updateHeaderBar(mediaInfo);

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
            this.controls.addRadioButtons(
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

    updateHeaderBar(mediaInfo)
    {
        if(!this.headerBar)
            return;

        let title = mediaInfo.get_title();
        let subtitle = mediaInfo.get_uri() || null;

        if(subtitle.startsWith('file://')) {
            subtitle = GLib.filename_from_uri(subtitle)[0];
            subtitle = GLib.path_get_basename(subtitle);
        }

        if(!title) {
            title = (!subtitle)
                ? this.defaultTitle
                : (subtitle.includes('.'))
                ? subtitle.split('.').slice(0, -1).join('.')
                : subtitle;

            subtitle = null;
        }

        this.headerBar.set_title(title);
        this.headerBar.set_subtitle(subtitle);

        this.fsTitle.label = title;
    }

    updateTime()
    {
        let currTime = GLib.DateTime.new_now_local();
        let endTime = currTime.add_seconds(
            this.controls.positionAdjustment.get_upper() - this.lastPositionValue
        );
        let now = currTime.format(this.timeFormat);

        this.fsTime.set_label(now);
        this.fsEndTime.set_label(`Ends at: ${endTime.format(this.timeFormat)}`);

        // Make sure that next timeout is always run after clock changes,
        // by delaying it for additional few milliseconds
        let nextUpdate = 60002 - parseInt(currTime.get_seconds() * 1000);
        debug(`updated current time: ${now}`);

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

            this.controls.addRadioButtons(
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
        // reenabling audio is slow (as expected),
        // so it is better to toggle mute instead
        if(type === 'audio') {
            if(activeId < 0)
                return this._player.set_mute(true);

            if(this._player.get_mute())
                this._player.set_mute(false);

            return this._player[`set_${type}_track`](activeId);
        }

        if(activeId < 0) {
            // disabling video leaves last frame frozen,
            // so we hide it by making it transparent
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
                this.controls.togglePlayButton.setPlayImage();
                break;
            case GstPlayer.PlayerState.PLAYING:
                this.controls.togglePlayButton.setPauseImage();
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

        this.controls.positionAdjustment.set_upper(duration);
        this.controls.positionAdjustment.set_step_increment(increment);
        this.controls.positionAdjustment.set_page_increment(increment);

        this.controls.durationFormated = this.controls._getFormatedTime(duration);
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

        if(this.controls.fullscreenMode)
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

        if(this.controls.volumeButton.image.icon_name !== iconName)
        {
            debug(`set volume icon: ${icon}`);
            this.controls.volumeButton.image.set_from_icon_name(
                iconName,
                this.controls.volumeButton.image.icon_size
            );
        }

        if(volume === this.lastVolumeValue)
            return;

        this.lastVolumeValue = volume;
        this._player.set_volume(volume);
    }
});
