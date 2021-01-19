const { Gdk, GLib, GObject, GstPlayer, Gtk } = imports.gi;
const { Controls } = imports.clapper_src.controls;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const { Player } = imports.clapper_src.player;
const Revealers = imports.clapper_src.revealers;

const { debug } = Debug;
const { settings } = Misc;

var Widget = GObject.registerClass(
class ClapperWidget extends Gtk.Grid
{
    _init()
    {
        super._init();

        /* load CSS here to allow using this class
         * separately as a pre-made GTK widget */
        Misc.loadCustomCss();

        this.windowSize = JSON.parse(settings.get_string('window-size'));
        this.floatSize = JSON.parse(settings.get_string('float-size'));

        this.fullscreenMode = false;
        this.floatingMode = false;
        this.isSeekable = false;

        this.needsTracksUpdate = true;

        this.overlay = new Gtk.Overlay();
        this.revealerTop = new Revealers.RevealerTop();
        this.revealerBottom = new Revealers.RevealerBottom();
        this.controls = new Controls();

        this.controlsBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
        });
        this.controlsBox.add_css_class('controlsbox');
        this.controlsBox.append(this.controls);

        this.attach(this.overlay, 0, 0, 1, 1);
        this.attach(this.controlsBox, 0, 1, 1, 1);

        this.mapSignal = this.connect('map', this._onMap.bind(this));

        this.player = new Player();
        this.controls.elapsedButton.scrolledWindow.set_child(this.player.playlistWidget);
        this.controls.speedAdjustment.bind_property(
            'value', this.player, 'rate', GObject.BindingFlags.BIDIRECTIONAL
        );

        this.player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));
        this.player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));

        /* FIXME: re-enable once ported to new GstPlayer API with messages bus */
        //this.player.connect('volume-changed', this._onPlayerVolumeChanged.bind(this));

        this.overlay.set_child(this.player.widget);
        this.overlay.add_overlay(this.revealerTop);
        this.overlay.add_overlay(this.revealerBottom);

        const motionController = new Gtk.EventControllerMotion();
        motionController.connect('leave', this._onLeave.bind(this));
        this.add_controller(motionController);

        const topClickGesture = new Gtk.GestureClick();
        topClickGesture.set_button(0);
        topClickGesture.connect('pressed', this.player._onWidgetPressed.bind(this.player));
        this.revealerTop.add_controller(topClickGesture);

        const topMotionController = new Gtk.EventControllerMotion();
        topMotionController.connect('motion', this.player._onWidgetMotion.bind(this.player));
        this.revealerTop.add_controller(topMotionController);

        const topScrollController = new Gtk.EventControllerScroll();
        topScrollController.set_flags(Gtk.EventControllerScrollFlags.BOTH_AXES);
        topScrollController.connect('scroll', this.player._onScroll.bind(this.player));
        this.revealerTop.add_controller(topScrollController);
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
        const root = this.get_root();
        if(!root) return;

        const un = (this.fullscreenMode) ? 'un' : '';
        root[`${un}fullscreen`]();
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.fullscreenMode === isFullscreen)
            return;

        this.fullscreenMode = isFullscreen;

        const root = this.get_root();
        const action = (isFullscreen) ? 'add' : 'remove';
        root[action + '_css_class']('gpufriendlyfs');

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

        const root = this.get_root();
        const size = (Misc.isOldGtk)
            ? root.get_size()
            : root.get_default_size();

        this._saveWindowSize(size);

        if(isFloating) {
            this.windowSize = size;
            this.player.widget.set_size_request(192, 108);
        }
        else {
            this.floatSize = size;
            this.player.widget.set_size_request(-1, -1);
        }

        this.floatingMode = isFloating;

        this.revealerBottom.setFloatingClass(isFloating);
        this._changeControlsPlacement(isFloating);
        this.controls.setFloatingMode(isFloating);
        this.controls.unfloatButton.set_visible(isFloating);
        this._setWindowFloating(isFloating);

        const resize = (isFloating)
            ? this.floatSize
            : this.windowSize;

        if(Misc.isOldGtk)
            root.resize(resize[0], resize[1]);
        else
            root.set_default_size(resize[0], resize[1]);

        debug(`resized window: ${resize[0]}x${resize[1]}`);

        this.revealerBottom.showChild(false);
        this.player.widget.grab_focus();
    }

    _setWindowFloating(isFloating)
    {
        const root = this.get_root();
        const cssClass = 'floatingwindow';

        if(isFloating === root.has_css_class(cssClass))
            return;

        const action = (isFloating) ? 'add' : 'remove';
        root[action + '_css_class'](cssClass);
    }

    _saveWindowSize(size)
    {
        const rootName = (this.floatingMode)
            ? 'float'
            : 'window';

        settings.set_string(`${rootName}-size`, JSON.stringify(size));
        debug(`saved ${rootName} size: ${size[0]}x${size[1]}`);
    }

    _changeControlsPlacement(isOnTop)
    {
        if(isOnTop) {
            this.controlsBox.remove(this.controls);
            this.revealerBottom.append(this.controls);
        }
        else {
            this.revealerBottom.remove(this.controls);
            this.controlsBox.append(this.controls);
        }

        this.controlsBox.set_visible(!isOnTop);
    }

    _updateMediaInfo()
    {
        const mediaInfo = this.player.get_media_info();
        if(!mediaInfo)
            return GLib.SOURCE_REMOVE;

        /* Set titlebar media title and path */
        this.updateTitles(mediaInfo);

        /* Show/hide position scale on LIVE */
        const isLive = mediaInfo.is_live();
        this.isSeekable = mediaInfo.is_seekable();
        this.controls.setLiveMode(isLive, this.isSeekable);

        if(this.player.needsTocUpdate) {
            /* FIXME: Remove `get_toc` check after required GstPlay(er) ver bump */
            if(!isLive && mediaInfo.get_toc)
                this.updateChapters(mediaInfo.get_toc());

            this.player.needsTocUpdate = false;
        }

        const streamList = mediaInfo.get_stream_list();
        const parsedInfo = {
            videoTracks: [],
            audioTracks: [],
            subtitleTracks: []
        };

        for(let info of streamList) {
            let type, text, codec;

            switch(info.constructor) {
                case GstPlayer.PlayerVideoInfo:
                    type = 'video';
                    codec = info.get_codec() || 'Undetermined';
                    text = codec + ', ' +
                        + info.get_width() + 'x'
                        + info.get_height();
                    let fps = info.get_framerate();
                    fps = Number((fps[0] / fps[1]).toFixed(2));
                    if(fps)
                        text += `@${fps}`;
                    break;
                case GstPlayer.PlayerAudioInfo:
                    type = 'audio';
                    codec = info.get_codec() || 'Undetermined';
                    if(codec.includes('(')) {
                        codec = codec.substring(
                            codec.indexOf('(') + 1,
                            codec.indexOf(')')
                        );
                    }
                    text = info.get_language() || 'Undetermined';
                    text += ', ' + codec + ', '
                        + info.get_channels() + ' Channels';
                    break;
                case GstPlayer.PlayerSubtitleInfo:
                    type = 'subtitle';
                    text = info.get_language() || 'Undetermined';
                    break;
                default:
                    debug(`unrecognized media info type: ${info.constructor}`);
                    break;
            }
            const tracksArr = parsedInfo[`${type}Tracks`];
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

        let anyButtonShown = false;

        for(let type of ['video', 'audio', 'subtitle']) {
            const currStream = this.player[`get_current_${type}_track`]();
            const activeId = (currStream) ? currStream.get_index() : -1;

            if(currStream && type !== 'subtitle') {
                const caps = currStream.get_caps();
                debug(`${type} caps: ${caps.to_string()}`, 'LEVEL_INFO');
            }
            if(type === 'video') {
                const isShowVis = (parsedInfo[`${type}Tracks`].length === 0);
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

            anyButtonShown = true;
        }
        this.controls.revealTracksRevealer.set_visible(anyButtonShown);

        return GLib.SOURCE_REMOVE;
    }

    updateTitles(mediaInfo)
    {
        let title = mediaInfo.get_title();
        let subtitle = this.player.playlistWidget.getActiveFilename();

        if(!title) {
            title = (subtitle.includes('.'))
                ? subtitle.split('.').slice(0, -1).join('.')
                : subtitle;

            subtitle = null;
        }

        const root = this.get_root();
        const headerbar = root.get_titlebar();

        if(headerbar && headerbar.updateHeaderBar)
            headerbar.updateHeaderBar(title, subtitle);

        this.revealerTop.setMediaTitle(title);
    }

    updateTime()
    {
        if(!this.revealerTop.visible)
            return null;

        const currTime = GLib.DateTime.new_now_local();
        const endTime = currTime.add_seconds(
            this.controls.positionAdjustment.get_upper() - this.controls.currentPosition
        );
        const nextUpdate = this.revealerTop.setTimes(currTime, endTime);

        return nextUpdate;
    }

    updateChapters(toc)
    {
        if(!toc) return;

        const entries = toc.get_entries();
        if(!entries) return;

        for(let entry of entries) {
            const subentries = entry.get_sub_entries();
            if(!subentries) continue;

            for(let subentry of subentries)
                this._parseTocSubentry(subentry);
        }
    }

    _parseTocSubentry(subentry)
    {
        const [success, start, stop] = subentry.get_start_stop_times();
        if(!success) {
            debug('could not obtain toc subentry start/stop times');
            return;
        }

        const pos = Math.floor(start / 1000000) / 1000;
        const tags = subentry.get_tags();

        this.controls.positionScale.add_mark(pos, Gtk.PositionType.TOP, null);
        this.controls.positionScale.add_mark(pos, Gtk.PositionType.BOTTOM, null);

        if(!tags) {
            debug('could not obtain toc subentry tags');
            return;
        }

        const [isString, title] = tags.get_string('title');
        if(!isString) {
            debug('toc subentry tag does not have a title');
            return;
        }

        if(!this.controls.chapters)
            this.controls.chapters = {};

        this.controls.chapters[pos] = title;
        debug(`chapter at ${pos}: ${title}`);
    }

    showVisualizationsButton(isShow)
    {
        if(isShow && !this.controls.visualizationsButton.isVisList) {
            debug('creating visualizations list');
            const visArr = GstPlayer.Player.visualizations_get();
            if(!visArr.length)
                return;

            const parsedVisArr = [{
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

        const action = (isShow) ? 'show' : 'hide';
        this.controls.visualizationsButton[action]();
        debug(`show visualizations button: ${isShow}`);
    }

    _onPlayerStateChanged(player, state)
    {
        switch(state) {
            case GstPlayer.PlayerState.BUFFERING:
                debug('player state changed to: BUFFERING');
                if(player.needsTocUpdate) {
                    this.controls._setChapterVisible(false);
                    this.controls.positionScale.clear_marks();
                    this.controls.chapters = null;
                }
                if(!player.playlistWidget.getActiveIsLocalFile()) {
                    this.needsTracksUpdate = true;
                }
                break;
            case GstPlayer.PlayerState.STOPPED:
                debug('player state changed to: STOPPED');
                this.controls.currentPosition = 0;
                this.controls.positionScale.set_value(0);
                this.controls.togglePlayButton.setPrimaryIcon();
                this.needsTracksUpdate = true;
                break;
            case GstPlayer.PlayerState.PAUSED:
                debug('player state changed to: PAUSED');
                this.controls.togglePlayButton.setPrimaryIcon();
                break;
            case GstPlayer.PlayerState.PLAYING:
                debug('player state changed to: PLAYING');
                this.controls.togglePlayButton.setSecondaryIcon();
                if(this.needsTracksUpdate) {
                    this.needsTracksUpdate = false;
                    GLib.idle_add(
                        GLib.PRIORITY_DEFAULT_IDLE,
                        this._updateMediaInfo.bind(this)
                    );
                }
                break;
            default:
                break;
        }

        const isNotStopped = (state !== GstPlayer.PlayerState.STOPPED);
        this.revealerTop.endTime.set_visible(isNotStopped);
    }

    _onPlayerDurationChanged(player)
    {
        const duration = Math.floor(player.get_duration() / 1000000000);

        /* Sometimes GstPlayer might re-emit
         * duration changed during playback */
        if(this.controls.currentDuration === duration)
            return;

        this.controls.currentDuration = duration;
        this.controls.showHours = (duration >= 3600);

        this.controls.positionAdjustment.set_upper(duration);
        this.controls.durationFormatted = Misc.getFormattedTime(duration);
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

        const positionSeconds = Math.round(position / 1000000000);
        if(positionSeconds === this.controls.currentPosition)
            return;

        this.controls.positionScale.set_value(positionSeconds);
    }

    _onPlayerVolumeChanged(player)
    {
        const volume = player.get_volume();

        /* FIXME: This check should not be needed, GstPlayer should not
         * emit 'volume-changed' with the same values, but it does. */
        if(volume === this.controls.currentVolume)
            return;

        /* Once above is fixed in GstPlayer, remove this var too */
        this.controls.currentVolume = volume;

        const cubicVolume = Misc.getCubicValue(volume);
        this.controls._updateVolumeButtonIcon(cubicVolume);
    }

    _onStateNotify(toplevel)
    {
        const isFullscreen = Boolean(
            toplevel.state & Gdk.ToplevelState.FULLSCREEN
        );

        if(this.fullscreenMode === isFullscreen)
            return;

        this.setFullscreenMode(isFullscreen);
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

        const root = this.get_root();
        if(!root) return;

        const surface = root.get_surface();
        surface.connect('notify::state', this._onStateNotify.bind(this));
    }
});
