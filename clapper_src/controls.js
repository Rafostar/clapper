const { GObject, Gdk, Gtk } = imports.gi;
const Buttons = imports.clapper_src.buttons;
const Debug = imports.clapper_src.debug;

const CONTROLS_MARGIN = 4;
const CONTROLS_SPACING = 4;

let { debug } = Debug;

var Controls = GObject.registerClass({
    Signals: {
        'position-seeking-changed': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
        'track-change-requested': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_INT]
        },
        'visualization-change-requested': {
            param_types: [GObject.TYPE_STRING]
        },
    }
}, class ClapperControls extends Gtk.HBox
{
    _init()
    {
        super._init({
            margin_left: CONTROLS_MARGIN,
            margin_right: CONTROLS_MARGIN,
            spacing: CONTROLS_SPACING,
            valign: Gtk.Align.END,
        });

        this.fullscreenMode = false;
        this.durationFormated = '00:00:00';
        this.buttonsArr = [];

        this._addTogglePlayButton();
        this._addPositionScale();
        this.visualizationsButton = this.addPopoverButton(
            'display-projector-symbolic'
        );
        this.videoTracksButton = this.addPopoverButton(
            'emblem-videos-symbolic'
        );
        this.audioTracksButton = this.addPopoverButton(
            'emblem-music-symbolic'
        );
        this.subtitleTracksButton = this.addPopoverButton(
            'media-view-subtitles-symbolic'
        );
        this._addVolumeButton();
        this.unfullscreenButton = this.addButton(
            'view-restore-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR,
            true
        );
        this.fullscreenButton = Gtk.Button.new_from_icon_name(
            'view-fullscreen-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR
        );
        this.setDefaultWidgetBehaviour(this.fullscreenButton);
        this.openMenuButton = Gtk.Button.new_from_icon_name(
            'open-menu-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR
        );
        this.setDefaultWidgetBehaviour(this.openMenuButton);
        this.forall(this.setDefaultWidgetBehaviour);

        this.realizeSignal = this.connect(
            'realize', this._onControlsRealize.bind(this)
        );
    }

    pack_start(widget, expand, fill, padding)
    {
        if(
            widget.box
            && widget.box.constructor
            && widget.box.constructor === Gtk.Box
        )
            widget = widget.box;

        super.pack_start(widget, expand, fill, padding);
    }

    setFullscreenMode(isFullscreen)
    {
        if(isFullscreen === this.fullscreenMode)
            return;

        for(let button of this.buttonsArr)
            button.setFullscreenMode(isFullscreen);

        this.fullscreenMode = isFullscreen;
    }

    addButton(iconName, size, noPack)
    {
        let button = new Buttons.BoxedIconButton(
            iconName, size, this.fullscreenMode
        );

        if(!noPack)
            this.pack_start(button, false, false, 0);

        this.buttonsArr.push(button);
        return button;
    }

    addPopoverButton(iconName, size)
    {
        let button = new Buttons.BoxedPopoverButton(
            iconName, size, this.fullscreenMode
        );
        this.pack_start(button, false, false, 0);
        this.buttonsArr.push(button);

        return button;
    }

    addRadioButtons(box, array, activeId)
    {
        let group = null;
        let children = box.get_children();
        let lastEl = (children.length > array.length)
            ? children.length
            : array.length;

        for(let i = 0; i < lastEl; i++) {
            if(i >= array.length) {
                children[i].hide();
                debug(`hiding unused ${children[i].type} radioButton nr: ${i}`);
                continue;
            }

            let el = array[i];
            let radioButton;

            if(i < children.length) {
                radioButton = children[i];
                debug(`reusing ${el.type} radioButton nr: ${i}`);
            }
            else {
                debug(`creating new ${el.type} radioButton nr: ${i}`);
                radioButton = new Gtk.RadioButton({
                    group: group,
                });
                radioButton.connect(
                    'toggled',
                    this._onRadioButtonToggled.bind(this, radioButton)
                );
                this.setDefaultWidgetBehaviour(radioButton);
                box.add(radioButton);
            }

            radioButton.label = el.label;
            debug(`radioButton label: ${radioButton.label}`);
            radioButton.type = el.type;
            debug(`radioButton type: ${radioButton.type}`);
            radioButton.activeId = el.activeId;
            debug(`radioButton id: ${radioButton.activeId}`);

            if(radioButton.activeId === activeId) {
                radioButton.set_active(true);
                debug(`activated ${el.type} radioButton nr: ${i}`);
            }
            if(!group)
                group = radioButton;

            radioButton.show();
        }
    }

    setDefaultWidgetBehaviour(widget)
    {
        widget.can_focus = false;
        widget.can_default = false;
    }

    setVolumeMarks(isAdded)
    {
        if(!isAdded)
            return this.volumeScale.clear_marks();

        this.volumeScale.add_mark(0, Gtk.PositionType.LEFT, '0%');
        this.volumeScale.add_mark(1, Gtk.PositionType.LEFT, '100%');
        this.volumeScale.add_mark(2, Gtk.PositionType.LEFT, '200%');
    }

    handleScaleIncrement(type, isUp)
    {
        let value = this[`${type}Scale`].get_value();
        let maxValue = this[`${type}Adjustment`].get_upper();
        let increment = this[`${type}Adjustment`].get_page_increment();

        value += (isUp) ? increment : -increment;
        value = (value < 0)
            ? 0
            : (value > maxValue)
            ? maxValue
            : value;

        this[`${type}Scale`].set_value(value);
    }

    _addTogglePlayButton()
    {
        this.togglePlayButton = this.addButton(
            'media-playback-start-symbolic',
            Gtk.IconSize.LARGE_TOOLBAR
        );
        this.togglePlayButton.setPlayImage = () =>
        {
            this.togglePlayButton.image.set_from_icon_name(
                'media-playback-start-symbolic',
                this.togglePlayButton.image.icon_size
            );
        }
        this.togglePlayButton.setPauseImage = () =>
        {
            this.togglePlayButton.image.set_from_icon_name(
                'media-playback-pause-symbolic',
                this.togglePlayButton.image.icon_size
            );
        }
    }

    _addPositionScale()
    {
        this.positionScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.LEFT,
            draw_value: true,
            hexpand: true,
        });
        let style = this.positionScale.get_style_context();
        style.add_class('positionscale');

        this.positionScale.connect(
            'format-value', this._onPositionScaleFormatValue.bind(this)
        );
        this.positionScale.connect(
            'button-press-event', this._onPositionScaleButtonPressEvent.bind(this)
        );
        this.positionScale.connect(
            'button-release-event', this._onPositionScaleButtonReleaseEvent.bind(this)
        );

        this.positionAdjustment = this.positionScale.get_adjustment();
        this.pack_start(this.positionScale, true, true, 0);
    }

    _addVolumeButton()
    {
        this.volumeButton = this.addPopoverButton(
            'audio-volume-muted-symbolic'
        );
        this.volumeButton.add_events(Gdk.EventMask.SCROLL_MASK);
        this.volumeButton.connect(
            'scroll-event', (self, event) => this._onScrollEvent(event)
        );
        this.volumeScale = new Gtk.Scale({
            orientation: Gtk.Orientation.VERTICAL,
            inverted: true,
            value_pos: Gtk.PositionType.TOP,
            draw_value: false,
            round_digits: 2,
            vexpand: true,
        });
        this.volumeScale.get_style_context().add_class('volumescale');
        this.volumeAdjustment = this.volumeScale.get_adjustment();

        this.volumeAdjustment.set_upper(2);
        this.volumeAdjustment.set_step_increment(0.05);
        this.volumeAdjustment.set_page_increment(0.05);
        this.setDefaultWidgetBehaviour(this.volumeScale);

        this.volumeButton.popoverBox.add(this.volumeScale);
        this.volumeButton.popoverBox.show_all();

        this.setVolumeMarks(true);
    }

    _getFormatedTime(time)
    {
        let hours = ('0' + Math.floor(time / 3600)).slice(-2);
	time -= hours * 3600;
	let minutes = ('0' + Math.floor(time / 60)).slice(-2);
	time -= minutes * 60;
	let seconds = ('0' + Math.floor(time)).slice(-2);

        return `${hours}:${minutes}:${seconds}`;
    }

    _onRadioButtonToggled(self, radioButton)
    {
        if(!radioButton.get_active())
            return;

        switch(radioButton.type) {
            case 'video':
            case 'audio':
            case 'subtitle':
                this.emit(
                    'track-change-requested',
                    radioButton.type,
                    radioButton.activeId
                );
                break;
            case 'visualization':
                this.emit(
                    `${radioButton.type}-change-requested`,
                    radioButton.activeId
                );
                break;
            default:
                break;
        }
    }

    _onPositionScaleFormatValue(self, value)
    {
        return this._getFormatedTime(value)
            + '/' + this.durationFormated;
    }

    _onPositionScaleButtonPressEvent()
    {
        this.isPositionSeeking = true;
        this.emit('position-seeking-changed', this.isPositionSeeking);
    }

    _onPositionScaleButtonReleaseEvent()
    {
        this.isPositionSeeking = false;
        this.emit('position-seeking-changed', this.isPositionSeeking);
    }

    _onControlsRealize()
    {
        this.disconnect(this.realizeSignal);

        let hiddenButtons = [
            'visualizations',
            'videoTracks',
            'audioTracks',
            'subtitleTracks'
        ];

        for(let name of hiddenButtons)
            this[`${name}Button`].hide();
    }

    _onScrollEvent(event)
    {
        let [res, direction] = event.get_scroll_direction();
        if(!res) return;

        let type = 'volume';

        switch(direction) {
            case Gdk.ScrollDirection.RIGHT:
            case Gdk.ScrollDirection.LEFT:
                type = 'position';
            case Gdk.ScrollDirection.UP:
            case Gdk.ScrollDirection.DOWN:
                let isUp = (
                    direction === Gdk.ScrollDirection.UP
                    || direction === Gdk.ScrollDirection.RIGHT
                );
                this.handleScaleIncrement(type, isUp);
                break;
            default:
                break;
        }
    }
});
