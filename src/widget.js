const { Gdk, Gio, GLib, GObject, Gst, GstClapper, Gtk } = imports.gi;
const { Controls } = imports.src.controls;
const Debug = imports.src.debug;
const Dialogs = imports.src.dialogs;
const Misc = imports.src.misc;
const { Player } = imports.src.player;
const Revealers = imports.src.revealers;

const { debug } = Debug;
const { settings } = Misc;

let lastTvScaling = null;

var Widget = GObject.registerClass({
    GTypeName: 'ClapperWidget',
},
class ClapperWidget extends Gtk.Grid
{
    _init()
    {
        super._init();

        /* load CSS here to allow using this class
         * separately as a pre-made GTK widget */
        Misc.loadCustomCss();

        this.posX = 0;
        this.posY = 0;
        this.layoutWidth = 0;

        this.isFullscreenMode = false;
        this.isMobileMonitor = false;

        this.isSeekable = false;
        this.isDragAllowed = false;
        this.isSwipePerformed = false;
        this.isReleaseKeyEnabled = false;
        this.isLongPressed = false;

        this.isCursorInPlayer = false;
        this.isPopoverOpen = false;

        this._hideControlsTimeout = null;
        this._updateTimeTimeout = null;
        this.surfaceMapSignal = null;

        this.needsCursorRestore = false;

        this.overlay = new Gtk.Overlay();
        this.revealerTop = new Revealers.RevealerTop();
        this.revealerBottom = new Revealers.RevealerBottom();
        this.controls = new Controls();

        this.controlsBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
        });
        this.controlsBox.add_css_class('controlsbox');
        this.controlsBox.append(this.controls);

        this.controlsRevealer = new Revealers.ControlsRevealer();
        this.controlsRevealer.set_child(this.controlsBox);

        this.attach(this.overlay, 0, 0, 1, 1);
        this.attach(this.controlsRevealer, 0, 1, 1, 1);

        this.player = new Player();
        const playerWidget = this.player.widget;

        this.controls.elapsedButton.scrolledWindow.set_child(this.player.playlistWidget);

        const speedAdjustment = this.controls.elapsedButton.speedScale.get_adjustment();
        speedAdjustment.bind_property(
            'value', this.player, 'rate', GObject.BindingFlags.BIDIRECTIONAL
        );

        const volumeAdjustment = this.controls.volumeButton.volumeScale.get_adjustment();
        volumeAdjustment.bind_property(
            'value', this.player, 'volume', GObject.BindingFlags.BIDIRECTIONAL
        );

        this.player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));
        this.player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));
        this.player.connect('media-info-updated', this._onMediaInfoUpdated.bind(this));

        this.player.connect('video-decoder-changed', this._onPlayerVideoDecoderChanged.bind(this));
        this.player.connect('audio-decoder-changed', this._onPlayerAudioDecoderChanged.bind(this));

        this.overlay.set_child(playerWidget);
        this.overlay.add_overlay(this.revealerTop);
        this.overlay.add_overlay(this.revealerBottom);

        const clickGesture = this._getClickGesture();
        playerWidget.add_controller(clickGesture);
        const clickGestureTop = this._getClickGesture();
        this.revealerTop.add_controller(clickGestureTop);

        const longPressGesture = this._getLongPressGesture();
        playerWidget.add_controller(longPressGesture);
        const longPressGestureTop = this._getLongPressGesture();
        this.revealerTop.add_controller(longPressGestureTop);

        const dragGesture = this._getDragGesture();
        playerWidget.add_controller(dragGesture);
        const dragGestureTop = this._getDragGesture();
        this.revealerTop.add_controller(dragGestureTop);

        const swipeGesture = this._getSwipeGesture();
        playerWidget.add_controller(swipeGesture);
        const swipeGestureTop = this._getSwipeGesture();
        this.revealerTop.add_controller(swipeGestureTop);

        const scrollController = this._getScrollController();
        playerWidget.add_controller(scrollController);
        const scrollControllerTop = this._getScrollController();
        this.revealerTop.add_controller(scrollControllerTop);

        const motionController = this._getMotionController();
        playerWidget.add_controller(motionController);
        const motionControllerTop = this._getMotionController();
        this.revealerTop.add_controller(motionControllerTop);

        const dropTarget = this._getDropTarget();
        playerWidget.add_controller(dropTarget);

        /* Applied only for widget to detect simple action key releases */
        const keyController = new Gtk.EventControllerKey();
        keyController.connect('key-released', this._onKeyReleased.bind(this));
        this.add_controller(keyController);
    }

    revealControls()
    {
        this.revealerTop.revealChild(true);
        this.revealerBottom.revealChild(true);

        this._checkSetUpdateTimeInterval();

        /* Reset timeout if already revealed, otherwise
         * timeout will be set after reveal finishes */
        if(this.revealerTop.child_revealed)
            this._setHideControlsTimeout();
    }

    toggleFullscreen()
    {
        const root = this.get_root();
        if(!root) return;

        const un = (this.isFullscreenMode) ? 'un' : '';
        root[`${un}fullscreen`]();
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreenMode === isFullscreen)
            return;

        debug('changing fullscreen mode');
        this.isFullscreenMode = isFullscreen;

        if(!isFullscreen)
            this._clearTimeout('updateTime');

        this.revealerTop.setFullscreenMode(isFullscreen, this.isMobileMonitor);
        this.revealerBottom.revealerBox.visible = isFullscreen;

        this._changeControlsPlacement(isFullscreen);
        this.controls.setFullscreenMode(isFullscreen, this.isMobileMonitor);

        if(this.revealerTop.child_revealed)
            this._checkSetUpdateTimeInterval();

        debug(`interface in fullscreen mode: ${isFullscreen}`);
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

    _onMediaInfoUpdated(player, mediaInfo)
    {
        /* Set titlebar media title */
        this.updateTitle(mediaInfo);

        /* FIXME: replace number with Gst.CLOCK_TIME_NONE when GJS
         * can do UINT64: https://gitlab.gnome.org/GNOME/gjs/-/merge_requests/524 */
        const isLive = (mediaInfo.is_live() || player.duration === 18446744073709552000);
        this.isSeekable = (!isLive && mediaInfo.is_seekable());

        /* Show/hide position scale on LIVE */
        this.controls.setLiveMode(isLive, this.isSeekable);

        /* Update remaining end time if visible */
        this.updateTime();

        if(this.player.needsTocUpdate) {
            if(!isLive)
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
                case GstClapper.ClapperVideoInfo:
                    type = 'video';
                    codec = info.get_codec() || _('Undetermined');
                    text = `${codec}, ${info.get_width()}×${info.get_height()}`;
                    let fps = info.get_framerate();
                    fps = Number((fps[0] / fps[1]).toFixed(2));
                    if(fps)
                        text += `@${fps}`;
                    break;
                case GstClapper.ClapperAudioInfo:
                    type = 'audio';
                    codec = info.get_codec() || _('Undetermined');
                    if(codec.includes('(')) {
                        codec = codec.substring(
                            codec.indexOf('(') + 1, codec.indexOf(')')
                        );
                    }
                    text = info.get_language() || _('Undetermined');
                    text += `, ${codec}, ${info.get_channels()} ` + _('Channels');
                    break;
                case GstClapper.ClapperSubtitleInfo:
                    type = 'subtitle';
                    const subsLang = info.get_language();
                    text = (subsLang) ? subsLang.split(',')[0] : _('Undetermined');
                    const subsTitle = Misc.getSubsTitle(info.get_title());
                    if(subsTitle)
                        text += `, ${subsTitle}`;
                    break;
                default:
                    debug(`unrecognized media info type: ${info.constructor}`);
                    break;
            }
            const tracksArr = parsedInfo[`${type}Tracks`];
            if(!tracksArr.length)
            {
                tracksArr[0] = {
                    label: _('Disabled'),
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
                if (caps)
                    debug(`${type} caps: ${caps.to_string()}`);
            }
            if(type === 'video') {
                const isShowVis = (
                    !parsedInfo.videoTracks.length
                    && parsedInfo.audioTracks.length
                );
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
    }

    updateTitle(mediaInfo)
    {
        let title = mediaInfo.get_title();

        if(!title) {
            const item = this.player.playlistWidget.getActiveRow();
            title = item.filename;
        }

        this.refreshWindowTitle(title);
        this.revealerTop.title = title;
        this.revealerTop.showTitle = true;
    }

    refreshWindowTitle(title)
    {
        const isFloating = !this.controlsRevealer.reveal_child;
        const pipSuffix = ' - PiP';
        const hasPipSuffix = title.endsWith(pipSuffix);

        this.root.title = (isFloating && !hasPipSuffix)
            ? title + pipSuffix
            : (!isFloating && hasPipSuffix)
            ? title.substring(0, title.length - pipSuffix.length)
            : title;
    }

    updateTime()
    {
        if(
            !this.revealerTop.visible
            || !this.revealerTop.revealerGrid.visible
            || !this.isFullscreenMode
            || this.isMobileMonitor
        )
            return null;

        const currTime = GLib.DateTime.new_now_local();
        const endTime = currTime.add_seconds(
            (this.controls.positionAdjustment.get_upper() - this.controls.currentPosition)
                / this.controls.elapsedButton.speedScale.get_value()
        );
        const nextUpdate = this.revealerTop.setTimes(currTime, endTime, this.isSeekable);

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

        const pos = Math.floor(start / Gst.MSECOND) / 1000;
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
            const visArr = GstClapper.Clapper.visualizations_get();
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
            case GstClapper.ClapperState.BUFFERING:
                debug('player state changed to: BUFFERING');
                if(player.needsTocUpdate) {
                    this.controls._setChapterVisible(false);
                    this.controls.positionScale.clear_marks();
                    this.controls.chapters = null;
                }
                break;
            case GstClapper.ClapperState.STOPPED:
                debug('player state changed to: STOPPED');
                this.controls.setInitialState();
                this.revealerTop.showTitle = false;
                break;
            case GstClapper.ClapperState.PAUSED:
                debug('player state changed to: PAUSED');
                this.controls.togglePlayButton.setPrimaryIcon();
                break;
            case GstClapper.ClapperState.PLAYING:
                debug('player state changed to: PLAYING');
                this.controls.togglePlayButton.setSecondaryIcon();
                break;
            default:
                break;
        }
    }

    _onPlayerDurationChanged(player, duration)
    {
        const durationSeconds = duration / Gst.SECOND;
        const durationFloor = Math.floor(durationSeconds);

        debug(`duration changed: ${durationSeconds}`);

        this.controls.showHours = (durationFloor >= 3600);
        this.controls.positionAdjustment.set_upper(durationFloor);
        this.controls.durationFormatted = Misc.getFormattedTime(durationFloor);
        this.controls.updateElapsedLabel();

        if(settings.get_boolean('resume-enabled')) {
            const resumeDatabase = JSON.parse(settings.get_string('resume-database'));
            const title = player.playlistWidget.getActiveFilename();

            debug(`searching database for resume info: ${title}`);

            const resumeInfo = resumeDatabase.find(info => {
                return (info.title === title && info.duration === durationSeconds);
            });

            if(resumeInfo) {
                debug('found resume info: ' + JSON.stringify(resumeInfo));
                new Dialogs.ResumeDialog(this.root, resumeInfo);

                const shrunkDatabase = resumeDatabase.filter(info => {
                    return !(info.title === title && info.duration === durationSeconds);
                });
                settings.set_string('resume-database', JSON.stringify(shrunkDatabase));
            }
            else
                debug('resume info not found');
        }
    }

    _onPlayerPositionUpdated(player, position)
    {
        if(
            !this.isSeekable
            || this.controls.isPositionDragging
            || !player.seekDone
        )
            return;

        const positionSeconds = Math.round(position / Gst.SECOND);
        if(positionSeconds === this.controls.currentPosition)
            return;

        this.controls.positionScale.set_value(positionSeconds);
    }

    _onPlayerVideoDecoderChanged(player, decoder)
    {
        this.controls.videoTracksButton.setDecoder(decoder);
    }

    _onPlayerAudioDecoderChanged(player, decoder)
    {
        this.controls.audioTracksButton.setDecoder(decoder);
    }

    _onStateNotify(toplevel)
    {
        const isMaximized = Boolean(
            toplevel.state & Gdk.ToplevelState.MAXIMIZED
        );
        const isFullscreen = Boolean(
            toplevel.state & Gdk.ToplevelState.FULLSCREEN
        );
        const headerBar = this.revealerTop.headerBar;

        headerBar.setMaximized(isMaximized);
        this.setFullscreenMode(isFullscreen);
    }

    _onLayoutUpdate(surface, width, height)
    {
        if(width === this.layoutWidth)
            return;

        /* Launch without showing revealers transitions on mobile width */
        if(!this.layoutWidth && width < this.controls.minFullViewWidth) {
            for(let revealer of this.controls.revealersArr)
                revealer.revealInstantly(false);
        }

        this.layoutWidth = width;

        if(this.isFullscreenMode)
            this.revealerBottom.setLayoutMargins(width);

        this.controls._onPlayerResize(width, height);
    }

    _onWindowMap(window)
    {
        const surface = window.get_surface();

        if(!surface.mapped)
            this.surfaceMapSignal = surface.connect(
                'notify::mapped', this._onSurfaceMapNotify.bind(this)
            );
        else
            this._onSurfaceMapNotify(surface);

        surface.connect('notify::state', this._onStateNotify.bind(this));
        surface.connect('enter-monitor', this._onEnterMonitor.bind(this));
        surface.connect('layout', this._onLayoutUpdate.bind(this));

        this.player._onWindowMap(window);
    }

    _onSurfaceMapNotify(surface)
    {
        if(!surface.mapped)
            return;

        if(this.surfaceMapSignal) {
            surface.disconnect(this.surfaceMapSignal);
            this.surfaceMapSignal = null;
        }

        const monitor = surface.display.get_monitor_at_surface(surface);
        const size = JSON.parse(settings.get_string('window-size'));
        const hasMonitor = Boolean(monitor && monitor.geometry);

        /* Let GTK handle window restore if no monitor, otherwise
           check if its size is greater then saved window size */
        if(
            !hasMonitor
            || (monitor.geometry.width >= size[0]
            && monitor.geometry.height >= size[1])
        ) {
            if(!hasMonitor)
                debug('restoring window size without monitor geometry');

            this.root.set_default_size(size[0], size[1]);
            debug(`restored window size: ${size[0]}x${size[1]}`);
        }
    }

    _onEnterMonitor(surface, monitor)
    {
        debug('entered new monitor');

        const { geometry } = monitor;
        debug(`monitor application-pixels: ${geometry.width}x${geometry.height}`);

        const monitorWidth = Math.max(geometry.width, geometry.height);
        this.isMobileMonitor = (monitorWidth < 1280);
        debug(`mobile monitor detected: ${this.isMobileMonitor}`);

        const hasTVCss = this.root.has_css_class('tvmode');
        if(hasTVCss === this.isMobileMonitor) {
            const action = (this.isMobileMonitor) ? 'remove' : 'add';
            this.root[action + '_css_class']('tvmode');
        }

        /* Mobile does not have TV mode, so we do not care about removing scaling */
        if(!this.isMobileMonitor) {
            const pixWidth = monitorWidth * monitor.scale_factor;
            const tvScaling = (pixWidth <= 1280)
                ? 'lowres'
                : (pixWidth > 1920)
                ? 'hires'
                : null;

            if(lastTvScaling !== tvScaling) {
                if(lastTvScaling)
                    this.root.remove_css_class(lastTvScaling);
                if(tvScaling)
                    this.root.add_css_class(tvScaling);

                lastTvScaling = tvScaling;
            }
            debug(`using scaling mode: ${tvScaling || 'normal'}`);
        }

        /* Update top revealer display mode */
        this.revealerTop.setFullscreenMode(this.isFullscreenMode, this.isMobileMonitor);
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

    _setHideControlsTimeout()
    {
        this._clearTimeout('hideControls');

        let time = 2500;

        if(this.isFullscreenMode && !this.isMobileMonitor)
            time += 1500;

        this._hideControlsTimeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, time, () => {
            this._hideControlsTimeout = null;

            if(this.isCursorInPlayer) {
                const blankCursor = Gdk.Cursor.new_from_name('none', null);

                this.player.widget.set_cursor(blankCursor);
                this.revealerTop.set_cursor(blankCursor);
                this.needsCursorRestore = true;
            }
            if(!this.isPopoverOpen) {
                this._clearTimeout('updateTime');

                this.revealerTop.revealChild(false);
                this.revealerBottom.revealChild(false);
            }

            return GLib.SOURCE_REMOVE;
        });
    }

    _checkSetUpdateTimeInterval()
    {
        if(
            this.isFullscreenMode
            && !this.isMobileMonitor
            && !this._updateTimeTimeout
        ) {
            debug('setting update time interval');
            this._setUpdateTimeInterval();
        }
    }

    _setUpdateTimeInterval()
    {
        this._clearTimeout('updateTime');

        const nextUpdate = this.updateTime();

        if(nextUpdate === null)
            return;

        this._updateTimeTimeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, nextUpdate, () => {
            this._updateTimeTimeout = null;

            if(this.isFullscreenMode)
                this._setUpdateTimeInterval();

            return GLib.SOURCE_REMOVE;
        });
    }

    _handleDoublePress(gesture, x, y)
    {
        if(!this.isFullscreenMode || !Misc.getIsTouch(gesture))
            return this.toggleFullscreen();

        const fieldSize = this.layoutWidth / 6;

        if(x < fieldSize) {
            debug('left side double press');
            this.player.playlistWidget.prevTrack();
        }
        else if(x > this.layoutWidth - fieldSize) {
            debug('right side double press');
            this.player.playlistWidget.nextTrack();
        }
        else {
            this.toggleFullscreen();
        }
    }

    _getClickGesture()
    {
        const clickGesture = new Gtk.GestureClick({
            button: 0,
            propagation_phase: Gtk.PropagationPhase.CAPTURE,
        });
        clickGesture.connect('pressed', this._onPressed.bind(this));
        clickGesture.connect('released', this._onReleased.bind(this));

        return clickGesture;
    }

    _getLongPressGesture()
    {
        const longPressGesture = new Gtk.GestureLongPress({
            touch_only: true,
            delay_factor: 0.9,
            propagation_phase: Gtk.PropagationPhase.CAPTURE,
        });
        longPressGesture.connect('pressed', this._onLongPressed.bind(this));

        return longPressGesture;
    }

    _getDragGesture()
    {
        const dragGesture = new Gtk.GestureDrag({
            propagation_phase: Gtk.PropagationPhase.CAPTURE,
        });
        dragGesture.connect('drag-update', this._onDragUpdate.bind(this));

        return dragGesture;
    }

    _getSwipeGesture()
    {
        const swipeGesture = new Gtk.GestureSwipe({
            touch_only: true,
            propagation_phase: Gtk.PropagationPhase.CAPTURE,
        });
        swipeGesture.connect('swipe', this._onSwipe.bind(this));
        swipeGesture.connect('update', this._onSwipeUpdate.bind(this));

        return swipeGesture;
    }

    _getScrollController()
    {
        const scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(Gtk.EventControllerScrollFlags.BOTH_AXES);
        scrollController.connect('scroll', this._onScroll.bind(this));

        return scrollController;
    }

    _getMotionController()
    {
        const motionController = new Gtk.EventControllerMotion();
        motionController.connect('enter', this._onEnter.bind(this));
        motionController.connect('leave', this._onLeave.bind(this));
        motionController.connect('motion', this._onMotion.bind(this));

        return motionController;
    }

    _getDropTarget()
    {
        const dropTarget = new Gtk.DropTarget({
            actions: Gdk.DragAction.COPY | Gdk.DragAction.MOVE,
        });
        dropTarget.set_gtypes([GObject.TYPE_STRING]);
        dropTarget.connect('motion', this._onDataMotion.bind(this));
        dropTarget.connect('drop', this._onDataDrop.bind(this));

        return dropTarget;
    }

    _getIsSwipeOk(velocity, otherVelocity)
    {
        if(!velocity)
            return false;

        const absVel = Math.abs(velocity);

        if(absVel < 20 || Math.abs(otherVelocity) * 1.5 >= absVel)
            return false;

        return this.isFullscreenMode;
    }

    _onPressed(gesture, nPress, x, y)
    {
        const button = gesture.get_current_button();
        const isDouble = (nPress % 2 == 0);

        this.isDragAllowed = !isDouble;
        this.isSwipePerformed = false;
        this.isLongPressed = false;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                if(isDouble)
                    this._handleDoublePress(gesture, x, y);
                break;
            case Gdk.BUTTON_SECONDARY:
                this.player.toggle_play();
                break;
            default:
                break;
        }
    }

    _onReleased(gesture, nPress, x, y)
    {
        /* Reveal if touch was not a swipe/long press or was already revealed */
        if(
            ((!this.isSwipePerformed && !this.isLongPressed)
            || this.revealerBottom.child_revealed)
            && Misc.getIsTouch(gesture)
        )
            this.revealControls();
    }

    _onLongPressed(gesture, x, y)
    {
        if(!this.isDragAllowed || !this.isFullscreenMode)
            return;

        this.isLongPressed = true;
        this.player.toggle_play();
    }

    _onKeyReleased(controller, keyval, keycode, state)
    {
        /* Ignore releases that did not trigger keypress
         * e.g. while holding left "Super" key */
        if(!this.isReleaseKeyEnabled)
            return;

        switch(keyval) {
            case Gdk.KEY_Right:
            case Gdk.KEY_Left:
                const value = Math.round(
                    this.controls.positionScale.get_value()
                );
                this.player.seek_seconds(value);
                this._setHideControlsTimeout();
                this.isReleaseKeyEnabled = false;
                break;
            default:
                break;
        }
    }

    _onDragUpdate(gesture, offsetX, offsetY)
    {
        if(!this.isDragAllowed || this.isFullscreenMode)
            return;

        const { gtk_double_click_distance } = this.get_settings();

        if (
            Math.abs(offsetX) > gtk_double_click_distance
            || Math.abs(offsetY) > gtk_double_click_distance
        ) {
            const [isActive, startX, startY] = gesture.get_start_point();
            if(!isActive) return;

            const playerWidget = this.player.widget;

            const native = playerWidget.get_native();
            if(!native) return;

            let [isShared, winX, winY] = playerWidget.translate_coordinates(
                native, startX, startY
            );
            if(!isShared) return;

            const [nativeX, nativeY] = native.get_surface_transform();
            winX += nativeX;
            winY += nativeY;

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

    _onSwipe(gesture, velocityX, velocityY)
    {
        if(!this._getIsSwipeOk(velocityX, velocityY))
            return;

        this._onScroll(gesture, -velocityX, 0);
        this.isSwipePerformed = true;
    }

    _onSwipeUpdate(gesture, sequence)
    {
        const [isCalc, velocityX, velocityY] = gesture.get_velocity();
        if(!isCalc) return;

        if(!this._getIsSwipeOk(velocityY, velocityX))
            return;

        const isIncrease = velocityY < 0;

        this.player.adjust_volume(isIncrease, 0.01);
        this.isSwipePerformed = true;
    }

    _onScroll(controller, dx, dy)
    {
        const isHorizontal = (Math.abs(dx) >= Math.abs(dy));
        const isIncrease = (isHorizontal) ? dx < 0 : dy < 0;

        if(isHorizontal) {
            this.player.adjust_position(isIncrease);
            const value = Math.round(this.controls.positionScale.get_value());
            this.player.seek_seconds(value);
        }
        else
            this.player.adjust_volume(isIncrease);

        return true;
    }

    _onEnter(controller, x, y)
    {
        this.isCursorInPlayer = true;
    }

    _onLeave(controller)
    {
        if(this.isFullscreenMode)
            return;

        this.isCursorInPlayer = false;
    }

    _onMotion(controller, posX, posY)
    {
        this.isCursorInPlayer = true;

        /* GTK4 sometimes generates motions with same coords */
        if(this.posX === posX && this.posY === posY)
            return;

        /* Do not show cursor on small movements */
        if(
            Math.abs(this.posX - posX) >= 0.5
            || Math.abs(this.posY - posY) >= 0.5
        ) {
            if(this.needsCursorRestore) {
                const defaultCursor = Gdk.Cursor.new_from_name('default', null);

                this.player.widget.set_cursor(defaultCursor);
                this.revealerTop.set_cursor(defaultCursor);
                this.needsCursorRestore = false;
            }
            this.revealControls();
        }

        this.posX = posX;
        this.posY = posY;
    }

    _onDataMotion(dropTarget, x, y)
    {
        return Gdk.DragAction.MOVE;
    }

    _onDataDrop(dropTarget, value, x, y)
    {
        const files = value.split(/\r?\n/).filter(uri => {
            return Gst.uri_is_valid(uri);
        });

        if(!files.length)
            return false;

        for(let index in files)
            files[index] = Gio.File.new_for_uri(files[index]);

        /* TODO: remove GTK < 4.3.2 compat someday */
        const currentDrop = dropTarget.current_drop || dropTarget.drop;
        const app = this.root.application;
        app.isFileAppend = Boolean(currentDrop.actions & Gdk.DragAction.COPY);
        app.open(files, "");

        return true;
    }
});
