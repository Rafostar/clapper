const { GLib, GObject, Gdk, Gtk } = imports.gi;
const Buttons = imports.src.buttons;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const Revealers = imports.src.revealers;

const { debug } = Debug;
const { settings } = Misc;

const INITIAL_ELAPSED = '00:00/00:00';

var Controls = GObject.registerClass(
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
        this.isFullscreen = false;

        this.showHours = false;
        this.durationFormatted = '00:00';
        this.buttonsArr = [];
        this.revealersArr = [];
        this.chapters = null;

        this.chapterShowId = null;
        this.chapterHideId = null;

        this._addTogglePlayButton();
        this._addElapsedButton();
        this._addPositionScale();

        const revealTracksButton = new Buttons.IconToggleButton(
            'go-previous-symbolic',
            'go-next-symbolic'
        );
        revealTracksButton.add_css_class('narrowbutton');
        this.buttonsArr.push(revealTracksButton);
        const tracksRevealer = new Revealers.ButtonsRevealer(
            'SLIDE_LEFT', revealTracksButton
        );
        this.visualizationsButton = this.addIconPopoverButton(
            'display-projector-symbolic',
            tracksRevealer
        );
        this.visualizationsButton.set_visible(false);
        this.videoTracksButton = this.addIconPopoverButton(
            'emblem-videos-symbolic',
            tracksRevealer
        );
        this.videoTracksButton.set_visible(false);
        this.audioTracksButton = this.addIconPopoverButton(
            'emblem-music-symbolic',
            tracksRevealer
        );
        this.audioTracksButton.set_visible(false);
        this.subtitleTracksButton = this.addIconPopoverButton(
            'media-view-subtitles-symbolic',
            tracksRevealer
        );
        this.subtitleTracksButton.set_visible(false);

        this.revealTracksRevealer = new Revealers.ButtonsRevealer('SLIDE_LEFT');
        this.revealTracksRevealer.append(revealTracksButton);
        this.revealTracksRevealer.set_visible(false);
        this.append(this.revealTracksRevealer);

        tracksRevealer.set_reveal_child(true);
        this.revealersArr.push(tracksRevealer);
        this.append(tracksRevealer);

        this._addVolumeButton();
        this.unfullscreenButton = this.addButton(
            'view-restore-symbolic'
        );
        this.unfullscreenButton.connect('clicked', this._onUnfullscreenClicked.bind(this));
        this.unfullscreenButton.set_visible(false);

        this.add_css_class('playercontrols');
        this.realizeSignal = this.connect('realize', this._onRealize.bind(this));
    }

    setFullscreenMode(isFullscreen)
    {
        /* Allow recheck on next resize */
        this.isMobile = null;

        for(let button of this.buttonsArr)
            button.setFullscreenMode(isFullscreen);

        this.unfullscreenButton.visible = isFullscreen;
        this.isFullscreen = isFullscreen;
    }

    setLiveMode(isLive, isSeekable)
    {
        if(isLive)
            this.elapsedButton.set_label('LIVE');

        this.positionScale.visible = isSeekable;
    }

    setInitialState()
    {
        this.currentPosition = 0;
        this.positionScale.set_value(0);
        this.positionScale.visible = false;

        this.elapsedButton.set_label(INITIAL_ELAPSED);
        this.togglePlayButton.setPrimaryIcon();

        for(let type of ['video', 'audio', 'subtitle'])
            this[`${type}TracksButton`].visible = false;

        this.visualizationsButton.visible = false;
    }

    updateElapsedLabel(value)
    {
        value = value || 0;

        const elapsed = Misc.getFormattedTime(value, this.showHours)
            + '/' + this.durationFormatted;

        this.elapsedButton.set_label(elapsed);
    }

    addButton(buttonIcon, revealer)
    {
        const button = (buttonIcon instanceof Gtk.Button)
            ? buttonIcon
            : new Buttons.CustomButton({ icon_name: buttonIcon });

        if(!revealer)
            this.append(button);
        else
            revealer.append(button);

        this.buttonsArr.push(button);

        return button;
    }

    addIconPopoverButton(iconName, revealer)
    {
        const button = new Buttons.IconPopoverButton(iconName);

        return this.addButton(button, revealer);
    }

    addLabelPopoverButton(text, revealer)
    {
        text = text || '';
        const button = new Buttons.LabelPopoverButton(text);

        return this.addButton(button, revealer);
    }

    addElapsedPopoverButton(text, revealer)
    {
        text = text || '';
        const button = new Buttons.ElapsedPopoverButton(text);

        return this.addButton(button, revealer);
    }

    addCheckButtons(box, array, activeId)
    {
        let group = null;
        let child = box.get_first_child();
        let i = 0;

        while(child || i < array.length) {
            if(i >= array.length) {
                child.hide();
                debug(`hiding unused ${child.type} checkButton nr: ${i}`);
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

        /* Reenabling audio is slow (as expected),
         * so it is better to toggle mute instead */
        if(checkButton.type === 'audio') {
            if(checkButton.activeId < 0)
                return clapperWidget.player.set_mute(true);

            if(clapperWidget.player.get_mute())
                clapperWidget.player.set_mute(false);

            return clapperWidget.player[
                `set_${checkButton.type}_track`
            ](checkButton.activeId);
        }

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

    _addTogglePlayButton()
    {
        this.togglePlayButton = new Buttons.IconToggleButton(
            'media-playback-start-symbolic',
            'media-playback-pause-symbolic'
        );
        this.togglePlayButton.child.add_css_class('playbackicon');
        this.togglePlayButton.connect(
            'clicked', this._onTogglePlayClicked.bind(this)
        );
        this.addButton(this.togglePlayButton);
    }

    _addElapsedButton()
    {
        const elapsedRevealer = new Revealers.ButtonsRevealer('SLIDE_RIGHT');
        this.elapsedButton = this.addElapsedPopoverButton(INITIAL_ELAPSED, elapsedRevealer);
        elapsedRevealer.set_reveal_child(true);
        this.revealersArr.push(elapsedRevealer);

        this.elapsedButton.addSeparator('Speed');
        const speedScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.BOTTOM,
            draw_value: false,
            round_digits: 2,
            hexpand: true,
            valign: Gtk.Align.CENTER,
        });
        speedScale.add_css_class('speedscale');

        this.speedAdjustment = speedScale.get_adjustment();
        this.speedAdjustment.set_lower(0.01);
        this.speedAdjustment.set_upper(2);
        this.speedAdjustment.set_value(1);
        this.speedAdjustment.set_page_increment(0.1);

        speedScale.add_mark(0.25, Gtk.PositionType.BOTTOM, '0.25x');
        speedScale.add_mark(1, Gtk.PositionType.BOTTOM, 'Normal');
        speedScale.add_mark(2, Gtk.PositionType.BOTTOM, '2x');

        this.elapsedButton.popoverBox.append(speedScale);
        this.append(elapsedRevealer);
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

        const box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });
        this.chapterPopover = new Gtk.Popover({
            position: Gtk.PositionType.TOP,
            autohide: false,
        });
        const chapterLabel = new Gtk.Label();
        chapterLabel.add_css_class('chapterlabel');
        this.chapterPopover.set_child(chapterLabel);
        this.chapterPopover.set_parent(box);

        box.append(this.positionScale);
        this.append(box);
    }

    _addVolumeButton()
    {
        this.volumeButton = this.addIconPopoverButton(
            'audio-volume-muted-symbolic'
        );
        this.volumeScale = new Gtk.Scale({
            orientation: Gtk.Orientation.VERTICAL,
            inverted: true,
            value_pos: Gtk.PositionType.TOP,
            draw_value: false,
            vexpand: true,
        });
        this.volumeScale.add_css_class('volumescale');
        this.volumeAdjustment = this.volumeScale.get_adjustment();

        this.volumeAdjustment.set_upper(Misc.maxVolume);
        this.volumeAdjustment.set_step_increment(0.05);
        this.volumeAdjustment.set_page_increment(0.05);

        for(let i of [0, 1, Misc.maxVolume]) {
            const text = (!i) ? '0%' : (i % 1 === 0) ? `${i}00%` : `${i * 10}0%`;
            this.volumeScale.add_mark(i, Gtk.PositionType.LEFT, text);
        }

        this.volumeScale.connect(
            'value-changed', this._onVolumeScaleValueChanged.bind(this)
        );
        this.volumeButton.popoverBox.append(this.volumeScale);
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
                    const scaleHeight = this.positionScale.parent.get_height();

                    this.chapterPopover.set_pointing_to(new Gdk.Rectangle({
                        x: -2,
                        y: -(controlsHeight - scaleHeight) / 2,
                        width: 2 * end,
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

    _onVolumeScaleValueChanged(scale)
    {
        const volume = scale.get_value();

        /* FIXME: All of below should be placed in 'volume-changed'
         *  event once we move to message bus API */
        const cssClass = 'overamp';
        const hasOveramp = (scale.has_css_class(cssClass));

        if(volume > 1) {
            if(!hasOveramp)
                scale.add_css_class(cssClass);
        }
        else {
            if(hasOveramp)
                scale.remove_css_class(cssClass);
        }

        const icon = (volume <= 0)
            ? 'muted'
            : (volume <= 0.3)
            ? 'low'
            : (volume <= 0.7)
            ? 'medium'
            : (volume <= 1)
            ? 'high'
            : 'overamplified';

        const iconName = `audio-volume-${icon}-symbolic`;
        if(this.volumeButton.icon_name === iconName)
            return;

        this.volumeButton.icon_name = iconName;
        debug(`set volume icon: ${icon}`);
    }

    _onPositionScaleDragging(scale)
    {
        const isPositionDragging = scale.has_css_class('dragging');

        /* When scale enters "fine-tune", slider changes position a little.
         * We do not want that to cause seek time change on TV mode */
        if(
            this.isFullscreen
            && !this.isMobile
            && scale.has_css_class('fine-tune')
        )
            scale.remove_css_class('fine-tune');

        if(this.isPositionDragging === isPositionDragging)
            return;

        const clapperWidget = this.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        if(this.isFullscreen) {
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

        for(let button of this.buttonsArr) {
            if(!button._onCloseRequest)
                continue;

            button._onCloseRequest();
        }

        this.chapterPopover.unparent();
    }
});
