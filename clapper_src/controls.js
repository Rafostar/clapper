const { GObject, Gdk, Gtk } = imports.gi;
const Buttons = imports.clapper_src.buttons;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;

const CONTROLS_MARGIN = 4;
const CONTROLS_SPACING = 4;

let { debug } = Debug;
let { settings } = Misc;

var Controls = GObject.registerClass(
class ClapperControls extends Gtk.Box
{
    _init()
    {
        super._init({
            orientation: Gtk.Orientation.HORIZONTAL,
            margin_start: CONTROLS_MARGIN,
            margin_end: CONTROLS_MARGIN,
            spacing: CONTROLS_SPACING,
            valign: Gtk.Align.END,
            can_focus: false,
        });

        this.currentVolume = 0;
        this.currentPosition = 0;
        this.currentDuration = 0;
        this.isPositionDragging = false;

        this.showHours = false;
        this.durationFormatted = '00:00';
        this.buttonsArr = [];

        this._addTogglePlayButton();
        this.elapsedButton = this.addLabelButton('00:00/00:00');
        this._addPositionScale();
        this.visualizationsButton = this.addPopoverButton(
            'display-projector-symbolic'
        );
        this.visualizationsButton.set_visible(false);
        this.videoTracksButton = this.addPopoverButton(
            'emblem-videos-symbolic'
        );
        this.videoTracksButton.set_visible(false);
        this.audioTracksButton = this.addPopoverButton(
            'emblem-music-symbolic'
        );
        this.audioTracksButton.set_visible(false);
        this.subtitleTracksButton = this.addPopoverButton(
            'media-view-subtitles-symbolic'
        );
        this.subtitleTracksButton.set_visible(false);
        this._addVolumeButton();
        this.unfullscreenButton = this.addButton(
            'view-restore-symbolic',
        );
        this.unfullscreenButton.connect('clicked', this._onUnfullscreenClicked.bind(this));
        this.unfullscreenButton.set_visible(false);

        this.unfloatButton = this.addButton(
            'preferences-desktop-remote-desktop-symbolic',
        );
        this.unfloatButton.connect('clicked', this._onUnfloatClicked.bind(this));
        this.unfloatButton.set_visible(false);

        let keyController = new Gtk.EventControllerKey();
        keyController.connect('key-pressed', this._onControlsKeyPressed.bind(this));
        keyController.connect('key-released', this._onControlsKeyReleased.bind(this));
        this.add_controller(keyController);

        this.add_css_class('playercontrols');
        this.realizeSignal = this.connect('realize', this._onRealize.bind(this));
    }

    setFullscreenMode(isFullscreen)
    {
        for(let button of this.buttonsArr)
            button.setFullscreenMode(isFullscreen);

        this.unfullscreenButton.set_visible(isFullscreen);
        this.set_can_focus(isFullscreen);
    }

    setFloatingMode(isFloating)
    {
        for(let button of this.buttonsArr)
            button.setFloatingMode(isFloating);
    }

    setLiveMode(isLive, isSeekable)
    {
        if(isLive)
            this.elapsedButton.set_label('LIVE');

        this.positionScale.visible = isSeekable;
    }

    updateElapsedLabel(value)
    {
        value = value || 0;

        let elapsed = Misc.getFormattedTime(value, this.showHours)
            + '/' + this.durationFormatted;

        this.elapsedButton.set_label(elapsed);
    }

    addButton(buttonIcon)
    {
        let button = (buttonIcon instanceof Gtk.Button)
            ? buttonIcon
            : new Buttons.IconButton(buttonIcon);

        this.append(button);
        this.buttonsArr.push(button);

        return button;
    }

    addLabelButton(text)
    {
        text = text || '';
        let button = new Buttons.LabelButton(text);

        return this.addButton(button);
    }

    addPopoverButton(iconName)
    {
        let button = new Buttons.PopoverButton(iconName);

        return this.addButton(button);
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

            let el = array[i];
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
        let clapperWidget = this.get_ancestor(Gtk.Grid);

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
            if(checkButton.type === 'video')
                clapperWidget.player.draw_black(true);

            return clapperWidget.player[
                `set_${checkButton.type}_track_enabled`
            ](false);
        }

        let setTrack = `set_${checkButton.type}_track`;

        clapperWidget.player[setTrack](checkButton.activeId);
        clapperWidget.player[`${setTrack}_enabled`](true);

        if(checkButton.type === 'video')
            clapperWidget.player.draw_black(false);
    }

    _handleVisualizationChange(checkButton)
    {
        let clapperWidget = this.get_ancestor(Gtk.Grid);
        let isEnabled = clapperWidget.player.get_visualization_enabled();

        if(!checkButton.activeId) {
            if(isEnabled) {
                clapperWidget.player.set_visualization_enabled(false);
                debug('disabled visualizations');
            }
            return;
        }

        let currVis = clapperWidget.player.get_current_visualization();

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
        this.togglePlayButton.add_css_class('playbackicon');
        this.togglePlayButton.connect(
            'clicked', this._onTogglePlayClicked.bind(this)
        );
        this.addButton(this.togglePlayButton);
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
        let scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(Gtk.EventControllerScrollFlags.BOTH_AXES);
        scrollController.connect('scroll', this._onPositionScaleScroll.bind(this));
        this.positionScale.add_controller(scrollController);

        this.positionScale.add_css_class('positionscale');
        this.positionScale.connect(
            'value-changed', this._onPositionScaleValueChanged.bind(this)
        );

        /* GTK4 is missing pressed/released signals for GtkRange/GtkScale.
         * We cannot add controllers, cause it already has them, so we
         * workaround this by observing css classes it currently has */
        this.positionScale.connect(
            'notify::css-classes', this._onPositionScaleDragging.bind(this)
        );

        this.positionAdjustment = this.positionScale.get_adjustment();
        this.positionAdjustment.set_page_increment(0);
        this.positionAdjustment.set_step_increment(8);

        let box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });
        box.append(this.positionScale);
        this.append(box);
    }

    _addVolumeButton()
    {
        this.volumeButton = this.addPopoverButton(
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
            let text = (!i) ? '0%' : (i % 1 === 0) ? `${i}00%` : `${i * 10}0%`;
            this.volumeScale.add_mark(i, Gtk.PositionType.LEFT, text);
        }

        this.volumeScale.connect(
            'value-changed', this._onVolumeScaleValueChanged.bind(this)
        );
        this.volumeButton.popoverBox.append(this.volumeScale);
    }

    _updateVolumeButtonIcon(volume)
    {
        let icon = (volume <= 0)
            ? 'muted'
            : (volume <= 0.3)
            ? 'low'
            : (volume <= 0.7)
            ? 'medium'
            : (volume <= 1)
            ? 'high'
            : 'overamplified';

        let iconName = `audio-volume-${icon}-symbolic`;
        if(this.volumeButton.icon_name === iconName)
            return;

        this.volumeButton.set_icon_name(iconName);
        debug(`set volume icon: ${icon}`);
    }

    _onRealize()
    {
        this.disconnect(this.realizeSignal);
        this.realizeSignal = null;

        let { player } = this.get_ancestor(Gtk.Grid);
        let scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(
            Gtk.EventControllerScrollFlags.VERTICAL
            | Gtk.EventControllerScrollFlags.DISCRETE
        );
        scrollController.connect('scroll', player._onScroll.bind(player));
        this.volumeButton.add_controller(scrollController);

        let initialVolume = (settings.get_string('volume-initial') === 'custom')
            ? settings.get_int('volume-value') / 100
            : Misc.getCubicValue(settings.get_double('volume-last'));

        this.volumeScale.set_value(initialVolume);
    }

    _onUnfullscreenClicked(button)
    {
        let root = button.get_root();
        root.unfullscreen();
    }

    _onUnfloatClicked(button)
    {
        let clapperWidget = this.get_ancestor(Gtk.Grid);
        clapperWidget.setFloatingMode(false);
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
        let { player } = this.get_ancestor(Gtk.Grid);
        player.toggle_play();
    }

    _onPositionScaleScroll(controller, dx, dy)
    {
        let { player } = this.get_ancestor(Gtk.Grid);
        player._onScroll(controller, dx || dy, 0);
    }

    _onPositionScaleValueChanged(scale)
    {
        let positionSeconds = Math.round(scale.get_value());

        this.currentPosition = positionSeconds;
        this.updateElapsedLabel(positionSeconds);
    }

    _onVolumeScaleValueChanged(scale)
    {
        let volume = scale.get_value();
        let linearVolume = Misc.getLinearValue(volume);
        let { player } = this.get_ancestor(Gtk.Grid);

        player.set_volume(linearVolume);

        /* FIXME: All of below should be placed in 'volume-changed'
         *  event once we move to message bus API */
        let cssClass = 'overamp';
        let hasOveramp = (scale.has_css_class(cssClass));

        if(volume > 1) {
            if(!hasOveramp)
                scale.add_css_class(cssClass);
        }
        else {
            if(hasOveramp)
                scale.remove_css_class(cssClass);
        }

        this._updateVolumeButtonIcon(volume);
    }

    _onPositionScaleDragging(scale)
    {
        let isPositionDragging = scale.has_css_class('dragging');

        if((this.isPositionDragging = isPositionDragging))
            return;

        let clapperWidget = this.get_ancestor(Gtk.Grid);
        if(!clapperWidget) return;

        let positionSeconds = Math.round(scale.get_value());
        clapperWidget.player.seek_seconds(positionSeconds);
    }

    /* Only happens when navigating through controls panel */
    _onControlsKeyPressed(controller, keyval, keycode, state)
    {
        let { player } = this.get_ancestor(Gtk.Grid);
        player._setHideControlsTimeout();
    }

    _onControlsKeyReleased(controller, keyval, keycode, state)
    {
        switch(keyval) {
            case Gdk.KEY_space:
            case Gdk.KEY_Return:
            case Gdk.KEY_Escape:
            case Gdk.KEY_Right:
            case Gdk.KEY_Left:
                break;
            default:
                let { player } = this.get_ancestor(Gtk.Grid);
                player._onWidgetKeyReleased(controller, keyval, keycode, state);
                break;
        }
    }

    _onCloseRequest()
    {
        for(let button of this.buttonsArr) {
            if(!button._onCloseRequest)
                continue;

            button._onCloseRequest();
        }
    }
});
