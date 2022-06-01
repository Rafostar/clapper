const { GLib, GObject, Gdk, Gtk } = imports.gi;
const Buttons = imports.src.buttons;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const Revealers = imports.src.revealers;

const { debug } = Debug;
const { settings } = Misc;

var Controls = GObject.registerClass({
    GTypeName: 'ClapperControls',
},
class ClapperControls extends Gtk.Box
{
    _init()
    {
        super._init({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.END,
            can_focus: false,
        });

        this.minFullViewWidth = 560;

        this.currentPosition = 0;
        this.isPositionDragging = false;
        this.isMobile = false;

        this.showHours = false;
        this.durationFormatted = `00${Misc.timeColon}00`;
        this.revealersArr = [];
        this.chapters = null;

        this.chapterShowId = null;
        this.chapterHideId = null;

        this.togglePlayButton = new Buttons.IconToggleButton(
            'play-symbolic',
            'pause-symbolic'
        );
        this.togglePlayButton.connect(
            'clicked', this._onTogglePlayClicked.bind(this)
        );
        this.append(this.togglePlayButton);

        const elapsedRevealer = new Revealers.ButtonsRevealer('SLIDE_RIGHT');
        this.elapsedButton = new Buttons.ElapsedTimeButton();
        elapsedRevealer.append(this.elapsedButton);
        elapsedRevealer.reveal_child = true;
        this.append(elapsedRevealer);
        this.revealersArr.push(elapsedRevealer);

        this._addPositionScale();

        const revealTracksButton = new Buttons.IconToggleButton(
            'go-previous-symbolic',
            'go-next-symbolic'
        );
        revealTracksButton.add_css_class('narrowbutton');
        const tracksRevealer = new Revealers.ButtonsRevealer(
            'SLIDE_LEFT', revealTracksButton
        );
        this.visualizationsButton = new Buttons.TrackSelectButton({
            icon_name: 'display-projector-symbolic',
            visible: false,
        });
        tracksRevealer.append(this.visualizationsButton);
        this.videoTracksButton = new Buttons.TrackSelectButton({
            icon_name: 'emblem-videos-symbolic',
            visible: false,
        });
        tracksRevealer.append(this.videoTracksButton);
        this.audioTracksButton = new Buttons.TrackSelectButton({
            icon_name: 'emblem-music-symbolic',
            visible: false,
        });
        tracksRevealer.append(this.audioTracksButton);
        this.subtitleTracksButton = new Buttons.TrackSelectButton({
            icon_name: 'media-view-subtitles-symbolic',
            visible: false,
        });
        tracksRevealer.append(this.subtitleTracksButton);

        this.revealTracksRevealer = new Revealers.ButtonsRevealer('SLIDE_LEFT');
        this.revealTracksRevealer.append(revealTracksButton);
        this.revealTracksRevealer.set_visible(false);
        this.append(this.revealTracksRevealer);

        tracksRevealer.set_reveal_child(true);
        this.revealersArr.push(tracksRevealer);
        this.append(tracksRevealer);

        this.volumeButton = new Buttons.VolumeButton();
        this.append(this.volumeButton);

        this.unfullscreenButton = new Buttons.CustomButton({
            icon_name: 'view-restore-symbolic',
        });
        this.unfullscreenButton.connect('clicked', this._onUnfullscreenClicked.bind(this));
        this.unfullscreenButton.set_visible(false);
        this.append(this.unfullscreenButton);

        this.add_css_class('clappercontrols');
        this.realizeSignal = this.connect('realize', this._onRealize.bind(this));
    }

    setFullscreenMode(isFullscreen, isMobileMonitor)
    {
        /* Allow recheck on next resize */
        this.isMobile = null;

        this.elapsedButton.setFullscreenMode(isFullscreen, isMobileMonitor);
        this.visualizationsButton.setFullscreenMode(isFullscreen, isMobileMonitor);
        this.videoTracksButton.setFullscreenMode(isFullscreen, isMobileMonitor);
        this.audioTracksButton.setFullscreenMode(isFullscreen, isMobileMonitor);
        this.subtitleTracksButton.setFullscreenMode(isFullscreen, isMobileMonitor);

        this.unfullscreenButton.visible = isFullscreen;
    }

    setLiveMode(isLive, isSeekable)
    {
        if(isLive)
            this.elapsedButton.label = 'LIVE';

        this.positionScale.visible = isSeekable;
    }

    setInitialState()
    {
        this.currentPosition = 0;
        this.positionScale.set_value(0);
        this.positionScale.visible = false;

        this.elapsedButton.setInitialState();
        this.togglePlayButton.setPrimaryIcon();

        for(let type of ['video', 'audio', 'subtitle'])
            this[`${type}TracksButton`].visible = false;

        this.visualizationsButton.visible = false;
    }

    updateElapsedLabel(value)
    {
        value = value || 0;

        const elapsed = Misc.getFormattedTime(value, this.showHours)
            + '∕' + this.durationFormatted;

        this.elapsedButton.label = elapsed;
    }

    addCheckButtons(box, array, activeId)
    {
        let group = null;
        let child = box.get_first_child();
        let i = 0;

        while(child || i < array.length) {
            if(i >= array.length) {
                if(child.visible) {
                    debug(`hiding unused ${child.type} checkButton nr: ${i}`);
                    child.visible = false;
                }
                i++;
                child = child.get_next_sibling();
                continue;
            }

            const el = array[i];
            let checkButton;

            if(child) {
                checkButton = child;
                debug(`reusing ${el.type} checkButton nr: ${i}`);
            }
            else {
                debug(`creating new ${el.type} checkButton nr: ${i}`);
                checkButton = new Gtk.CheckButton({
                    group: group,
                });
                checkButton.connect(
                    'toggled',
                    this._onCheckButtonToggled.bind(this)
                );
                box.append(checkButton);
            }

            checkButton.label = el.label;
            debug(`checkButton label: ${checkButton.label}`);
            checkButton.type = el.type;
            debug(`checkButton type: ${checkButton.type}`);
            checkButton.activeId = el.activeId;
            debug(`checkButton id: ${checkButton.activeId}`);
            checkButton.visible = true;

            if(checkButton.activeId === activeId) {
                checkButton.set_active(true);
                debug(`activated ${el.type} checkButton nr: ${i}`);
            }
            if(!group)
                group = checkButton;

            i++;
            if(child)
                child = child.get_next_sibling();
        }
    }

    _handleTrackChange(checkButton)
    {
        const clapperWidget = this.get_ancestor(Gtk.Grid);

        if(checkButton.activeId < 0) {
            return clapperWidget.player[
                `set_${checkButton.type}_track_enabled`
            ](false);
        }

        const setTrack = `set_${checkButton.type}_track`;

        clapperWidget.player[setTrack](checkButton.activeId);
        clapperWidget.player[`${setTrack}_enabled`](true);
    }

    _handleVisualizationChange(checkButton)
    {
        const clapperWidget = this.get_ancestor(Gtk.Grid);
        const isEnabled = clapperWidget.player.get_visualization_enabled();

        if(!checkButton.activeId) {
            if(isEnabled) {
                clapperWidget.player.set_visualization_enabled(false);
                debug('disabled visualizations');
            }
            return;
        }

        const currVis = clapperWidget.player.get_current_visualization();

        if(currVis === checkButton.activeId)
            return;

        debug(`set visualization: ${checkButton.activeId}`);
        clapperWidget.player.set_visualization(checkButton.activeId);

        if(!isEnabled) {
            clapperWidget.player.set_visualization_enabled(true);
            debug('enabled visualizations');
        }
    }

    _addPositionScale()
    {
        this.positionScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.LEFT,
            draw_value: false,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            can_focus: false,
            visible: false,
        });
        const scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(Gtk.EventControllerScrollFlags.BOTH_AXES);
        scrollController.connect('scroll', this._onPositionScaleScroll.bind(this));
        this.positionScale.add_controller(scrollController);

        this.positionScale.add_css_class('positionscale');
        this.positionScaleValueSignal = this.positionScale.connect(
            'value-changed', this._onPositionScaleValueChanged.bind(this)
        );

        /* GTK4 is missing pressed/released signals for GtkRange/GtkScale.
         * We cannot add controllers, cause it already has them, so we
         * workaround this by observing css classes it currently has */
        this.positionScaleDragSignal = this.positionScale.connect(
            'notify::css-classes', this._onPositionScaleDragging.bind(this)
        );

        this.positionAdjustment = this.positionScale.get_adjustment();
        this.positionAdjustment.set_page_increment(0);
        this.positionAdjustment.set_step_increment(8);

        const chapterLabel = new Gtk.Label();
        chapterLabel.add_css_class('chapterlabel');

        this.chapterPopover = new Gtk.Popover({
            position: Gtk.PositionType.TOP,
            autohide: false,
            child: chapterLabel,
        });
        const box = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });

        box.append(this.chapterPopover);
        box.append(this.positionScale);
        this.append(box);
    }

    _setChapterVisible(isVisible)
    {
        const type = (isVisible) ? 'Show' : 'Hide';
        const anti = (isVisible) ? 'Hide' : 'Show';

        if(this[`chapter${anti}Id`]) {
            GLib.source_remove(this[`chapter${anti}Id`]);
            this[`chapter${anti}Id`] = null;
        }

        if(
            this[`chapter${type}Id`]
            || (!isVisible && this.chapterPopover.visible === isVisible)
        )
            return;

        debug(`changing chapter visibility to: ${isVisible}`);

        this[`chapter${type}Id`] = GLib.idle_add(
            GLib.PRIORITY_DEFAULT_IDLE + 20,
            () => {
                if(isVisible) {
                    const [start, end] = this.positionScale.get_slider_range();
                    const controlsHeight = this.parent.get_height();
                    const scaleBoxHeight = this.positionScale.parent.get_height();
                    const [isShared, destX, destY] = this.positionScale.translate_coordinates(
                        this.positionScale.parent, 0, 0
                    );
                    const clapperWidget = this.get_ancestor(Gtk.Grid);

                    /* Half of slider width, values are defined in CSS */
                    const sliderOffset = (
                        clapperWidget.isFullscreenMode && !clapperWidget.isMobileMonitor
                    ) ? 10 : 9;

                    this.chapterPopover.set_pointing_to(new Gdk.Rectangle({
                        x: destX + end - sliderOffset,
                        y: -(controlsHeight - scaleBoxHeight) / 2,
                        width: 0,
                        height: 0,
                    }));
                }

                this.chapterPopover.visible = isVisible;
                this[`chapter${type}Id`] = null;

                debug(`chapter visible: ${isVisible}`);

                return GLib.SOURCE_REMOVE;
            }
        );
    }

    _onRealize()
    {
        this.disconnect(this.realizeSignal);
        this.realizeSignal = null;

        const clapperWidget = this.get_ancestor(Gtk.Grid);
        const scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(
            Gtk.EventControllerScrollFlags.VERTICAL
            | Gtk.EventControllerScrollFlags.DISCRETE
        );
        scrollController.connect('scroll', clapperWidget._onScroll.bind(clapperWidget));
        this.volumeButton.add_controller(scrollController);

        const initialVolume = (settings.get_boolean('volume-custom'))
            ? settings.get_int('volume-value') / 100
            : settings.get_double('volume-last');

        clapperWidget.player.volume = initialVolume;
        clapperWidget.player.bind_property('mute', this.volumeButton, 'muted',
            GObject.BindingFlags.DEFAULT
        );
    }

    _onPlayerResize(width, height)
    {
        const isMobile = (width < this.minFullViewWidth);
        if(this.isMobile === isMobile)
            return;

        for(let revealer of this.revealersArr)
            revealer.set_reveal_child(!isMobile);

        this.revealTracksRevealer.set_reveal_child(isMobile);
        this.isMobile = isMobile;
    }

    _onUnfullscreenClicked(button)
    {
        const root = button.get_root();
        root.unfullscreen();
    }

    _onCheckButtonToggled(checkButton)
    {
        if(!checkButton.get_active())
            return;

        switch(checkButton.type) {
            case 'video':
            case 'audio':
            case 'subtitle':
                this._handleTrackChange(checkButton);
                break;
            case 'visualization':
                this._handleVisualizationChange(checkButton);
                break;
            default:
                break;
        }
    }

    _onTogglePlayClicked()
    {
        /* Parent of controls changes, so get ancestor instead */
        const { player } = this.get_ancestor(Gtk.Grid);
        player.toggle_play();
    }

    _onPositionScaleScroll(controller, dx, dy)
    {
        const clapperWidget = this.get_ancestor(Gtk.Grid);
        clapperWidget._onScroll(controller, dx || dy, 0);
    }

    _onPositionScaleValueChanged(scale)
    {
        const scaleValue = scale.get_value();
        const positionSeconds = Math.round(scaleValue);

        this.currentPosition = positionSeconds;
        this.updateElapsedLabel(positionSeconds);

        if(this.chapters && this.isPositionDragging) {
            const chapter = this.chapters[scaleValue];
            const isChapter = (chapter != null);

            if(isChapter)
                this.chapterPopover.child.label = chapter;

            this._setChapterVisible(isChapter);
        }
    }

    _onPositionScaleDragging(scale)
    {
        const isPositionDragging = scale.has_css_class('dragging');

        if(this.isPositionDragging === isPositionDragging)
            return;

        const clapperWidget = this.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        if(clapperWidget.isFullscreenMode) {
            clapperWidget.revealControls();

            if(isPositionDragging)
                clapperWidget._clearTimeout('hideControls');
        }

        if((this.isPositionDragging = isPositionDragging))
            return;

        const scaleValue = scale.get_value();
        const isChapterSeek = this.chapterPopover.visible;

        if(!isChapterSeek) {
            const positionSeconds = Math.round(scaleValue);
            clapperWidget.player.seek_seconds(positionSeconds);
        }
        else {
            clapperWidget.player.seek_chapter(scaleValue);
            this._setChapterVisible(false);
        }
    }

    _onCloseRequest()
    {
        debug('controls close request');

        this.positionScale.disconnect(this.positionScaleValueSignal);
        this.positionScale.disconnect(this.positionScaleDragSignal);
    }
});
