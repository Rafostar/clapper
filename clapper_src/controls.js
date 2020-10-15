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
}, class ClapperControls extends Gtk.Box
{
    _init()
    {
        super._init({
            orientation: Gtk.Orientation.HORIZONTAL,
            margin_start: CONTROLS_MARGIN,
            margin_end: CONTROLS_MARGIN,
            spacing: CONTROLS_SPACING,
            valign: Gtk.Align.END,
        });

        this.durationFormated = '00:00:00';
        this.elapsedInitial = '00:00:00/00:00:00';
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
        );
        this.unfullscreenButton.set_visible(false);

        this.add_css_class('playercontrols');

        this.realizeSignal = this.connect('realize', this._onRealize.bind(this));
        this.destroySignal = this.connect('destroy', this._onDestroy.bind(this));
    }

    setFullscreenMode(isFullscreen)
    {
        for(let button of this.buttonsArr)
            button.setFullscreenMode(isFullscreen);

        this.unfullscreenButton.set_visible(isFullscreen);
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
                    this._onCheckButtonToggled.bind(this, checkButton)
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
        this.togglePlayButton = new Buttons.IconToggleButton(
            'media-playback-start-symbolic',
            'media-playback-pause-symbolic'
        );
        this.togglePlayButton.add_css_class('playbackicon');
        this.addButton(this.togglePlayButton);
    }

    _addPositionScale()
    {
        this.elapsedButton = this.addLabelButton(this.elapsedInitial);
        this.positionScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.LEFT,
            draw_value: false,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });

        this.togglePlayButton.bind_property('margin_top',
            this.positionScale, 'margin_top', GObject.BindingFlags.SYNC_CREATE
        );
        this.togglePlayButton.bind_property('margin_bottom',
            this.positionScale, 'margin_bottom', GObject.BindingFlags.SYNC_CREATE
        );

        this.positionScale.add_css_class('positionscale');
        this.positionScale.connect('value-changed', this._onPositionScaleValueChanged.bind(this));

        /* GTK4 is missing pressed/released signals for GtkRange/GtkScale.
         * We cannot add controllers, cause it already has them, so we
         * workaround this by observing css classes it currently has */
        this.positionScale.connect(
            'notify::css-classes', this._onPositionScaleDragging.bind(this)
        );

        this.positionAdjustment = this.positionScale.get_adjustment();
        this.append(this.positionScale);
    }

    _addVolumeButton()
    {
        this.volumeButton = this.addPopoverButton(
            'audio-volume-muted-symbolic'
        );
        let scrollController = new Gtk.EventControllerScroll();
        scrollController.set_flags(
            Gtk.EventControllerScrollFlags.VERTICAL
            | Gtk.EventControllerScrollFlags.DISCRETE
        );
        scrollController.connect('scroll', this._onScroll.bind(this));
        this.volumeButton.add_controller(scrollController);

        this.volumeScale = new Gtk.Scale({
            orientation: Gtk.Orientation.VERTICAL,
            inverted: true,
            value_pos: Gtk.PositionType.TOP,
            draw_value: false,
            round_digits: 2,
            vexpand: true,
        });
        this.volumeScale.add_css_class('volumescale');
        this.volumeAdjustment = this.volumeScale.get_adjustment();

        this.volumeAdjustment.set_upper(2);
        this.volumeAdjustment.set_step_increment(0.05);
        this.volumeAdjustment.set_page_increment(0.05);

        for(let i = 0; i <= 2; i++) {
            let text = (i) ? `${i}00%` : '0%';
            this.volumeScale.add_mark(i, Gtk.PositionType.LEFT, text);
        }
        this.volumeButton.popoverBox.append(this.volumeScale);
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

    _onCheckButtonToggled(self, checkButton)
    {
        if(!checkButton.get_active())
            return;

        switch(checkButton.type) {
            case 'video':
            case 'audio':
            case 'subtitle':
                this.emit(
                    'track-change-requested',
                    checkButton.type,
                    checkButton.activeId
                );
                break;
            case 'visualization':
                this.emit(
                    `${checkButton.type}-change-requested`,
                    checkButton.activeId
                );
                break;
            default:
                break;
        }
    }

    _onPositionScaleValueChanged()
    {
        let elapsed = this._getFormatedTime(this.positionScale.get_value())
            + '/' + this.durationFormated;

        this.elapsedButton.set_label(elapsed);
    }

    _onPositionScaleDragging(scale)
    {
        let isPositionSeeking = scale.has_css_class('dragging');

        if(this.isPositionSeeking === isPositionSeeking)
            return;

        this.isPositionSeeking = isPositionSeeking;
        this.emit('position-seeking-changed', this.isPositionSeeking);
    }

    _onRealize()
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

    _onScroll(controller, dx, dy)
    {
        let isVertical = Math.abs(dy) >= Math.abs(dx);
        let isIncrease = (isVertical) ? dy < 0 : dx < 0;
        let type = (isVertical) ? 'volume' : 'position';

        this.handleScaleIncrement(type, isIncrease);

        return true;
    }

    _onDestroy()
    {
        this.disconnect(this.destroySignal);

        this.visualizationsButton.emit('destroy');
        this.videoTracksButton.emit('destroy');
        this.audioTracksButton.emit('destroy');
        this.subtitleTracksButton.emit('destroy');
        this.volumeButton.emit('destroy');
    }
});
