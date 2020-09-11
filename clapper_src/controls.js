const { GObject, Gtk } = imports.gi;
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

        this._fullscreenMode = false;
        this.durationFormated = '00:00:00';
        this.buttonImages = [];

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

    set fullscreenMode(isFullscreen)
    {
        if(isFullscreen === this._fullscreenMode)
            return;

        for(let image of this.buttonImages) {
            image.icon_size = (isFullscreen)
                ? image.fullscreenSize
                : image.defaultSize;
        }

        this._fullscreenMode = isFullscreen;
    }

    get fullscreenMode()
    {
        return this._fullscreenMode;
    }

    addButton(iconName, size, noPack)
    {
        size = size || Gtk.IconSize.SMALL_TOOLBAR;

        let button = Gtk.Button.new_from_icon_name(iconName, size);
        let box = new Gtk.Box();

        button.margin_top = CONTROLS_MARGIN;
        button.margin_bottom = CONTROLS_MARGIN;
        button.image.defaultSize = size;
        button.image.fullscreenSize = (size === Gtk.IconSize.SMALL_TOOLBAR)
            ? Gtk.IconSize.LARGE_TOOLBAR
            : Gtk.IconSize.DND;

        this.setDefaultWidgetBehaviour(button);
        button.get_style_context().add_class('flat');

        if(!noPack) {
            box.pack_start(button, false, false, 0);
            this.pack_start(box, false, false, 0);
            box.show_all();
        }

        this.buttonImages.push(button.image);
        return button;
    }

    addPopoverButton(iconName, size)
    {
        let button = this.addButton(iconName, size);

        button.popover = new Gtk.Popover({
            relative_to: button.get_parent()
        });
        button.popoverBox = new Gtk.VBox();
        button.osd = this.fullscreenMode;
        button.popover.add(button.popoverBox);
        button.connect('clicked', this._onPopoverButtonClicked.bind(this, button));
        button.popoverBox.show();

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

    _onPopoverButtonClicked(self, button)
    {
        if(button.osd !== this.fullscreenMode) {
            let action = (this.fullscreenMode) ? 'add_class' : 'remove_class';
            button.popover.get_style_context()[action]('osd');
            button.osd = this.fullscreenMode;
        }
        button.popover.popup();
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
});
