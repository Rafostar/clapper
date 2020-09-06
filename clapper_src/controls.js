const { GObject, Gtk } = imports.gi;

var Controls = GObject.registerClass({
    Signals: {
        'position-seeking-changed': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
        'track-change-requested': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_INT]
        },
    }
}, class ClapperControls extends Gtk.HBox
{
    _init()
    {
        super._init({
            margin: 4,
            spacing: 4,
            valign: Gtk.Align.END,
        });

        this._fullscreenMode = false;
        this.durationFormated = '00:00:00';
        this.buttonImages = [];

        this.togglePlayButton = this.addButton(
            'media-playback-pause-symbolic',
            Gtk.IconSize.LARGE_TOOLBAR
        );
        this.pauseButton = this.addButton(
            'media-playback-start-symbolic',
            Gtk.IconSize.LARGE_TOOLBAR,
            true
        );
        this.playImage = this.pauseButton.image;
        this.pauseImage = this.togglePlayButton.image;

        this.positionScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.LEFT,
            draw_value: true,
            hexpand: true,
        });
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

        this.videoTracksButton = this.addPopoverButton(
            'emblem-videos-symbolic'
        );
        this.audioTracksButton = this.addPopoverButton(
            'emblem-music-symbolic'
        );
        this.subtitleTracksButton = this.addPopoverButton(
            'media-view-subtitles-symbolic'
        );

        this.volumeButton = new Gtk.ScaleButton({
            icons: [
                'audio-volume-muted-symbolic',
                'audio-volume-overamplified-symbolic',
                'audio-volume-low-symbolic',
                'audio-volume-medium-symbolic',
                'audio-volume-high-symbolic',
                'audio-volume-overamplified-symbolic',
                'audio-volume-overamplified-symbolic',
                'audio-volume-overamplified-symbolic',
            ],
            size: Gtk.IconSize.SMALL_TOOLBAR
        });
        let style = this.volumeButton.get_style_context();
        let styleStr = style.to_string(Gtk.StyleContextPrintFlags.SHOW_STYLE);

        if(!styleStr.includes('flat'))
            style.add_class('flat');

        this.volumeButtonImage = this.volumeButton.get_child();
        this.volumeButtonImage.defaultSize = Gtk.IconSize.SMALL_TOOLBAR;
        this.volumeButtonImage.fullscreenSize = Gtk.IconSize.LARGE_TOOLBAR;
        this.buttonImages.push(this.volumeButtonImage);

        this.volumeAdjustment = this.volumeButton.get_adjustment();
        this._prepareVolumeButton();
        this.pack_start(this.volumeButton, false, false, 0);

        this.toggleFullscreenButton = this.addButton(
            'view-fullscreen-symbolic'
        );
        this.unfullscreenButton = this.addButton(
            'view-restore-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR,
            true
        );
        this.fullscreenImage = this.toggleFullscreenButton.image;
        this.unfullscreenImage = this.unfullscreenButton.image;

        this.forall(this.setDefaultWidgetBehaviour);
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

        this.volumeButton.size = this.volumeButtonImage.icon_size;
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
        button.image.defaultSize = size;
        button.image.fullscreenSize = (size === Gtk.IconSize.SMALL_TOOLBAR)
            ? Gtk.IconSize.LARGE_TOOLBAR
            : Gtk.IconSize.DND;

        this.setDefaultWidgetBehaviour(button);
        button.get_style_context().add_class('flat');

        if(!noPack) {
            this.pack_start(button, false, false, 0);
            button.show();
        }

        this.buttonImages.push(button.image);
        return button;
    }

    addPopoverButton(iconName, size)
    {
        let button = this.addButton(iconName, size);

        button.popover = new Gtk.Popover({
            relative_to: button
        });
        button.popoverBox = new Gtk.VBox({
            margin_top: 4,
            margin_bottom: 4,
        });
        button.osd = this.fullscreenMode;
        button.popover.add(button.popoverBox);
        button.connect('clicked', this._onPopoverButtonClicked.bind(this, button));

        return button;
    }

    addRadioButtons(box, array, activeId)
    {
        let group = null;

        for(let el of array) {
            let radioButton = new Gtk.RadioButton({
                label: el.label,
                group: group,
            });
            radioButton.trackType = el.type;
            radioButton.trackId = el.value;

            if(radioButton.trackId === activeId)
                radioButton.set_active(true);
            if(!group)
                group = radioButton;

            radioButton.connect(
                'toggled', this._onTrackRadioButtonToggled.bind(this, radioButton)
            );
            this.setDefaultWidgetBehaviour(radioButton);
            box.add(radioButton);
        }
        box.show_all();
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

    _prepareVolumeButton()
    {
        this.volumeAdjustment.set_upper(2.001);
        this.volumeAdjustment.set_step_increment(0.05);
        this.volumeAdjustment.set_page_increment(0.05);

        this.volumeButton.popover = this.volumeButton.get_popup();
        this.volumeButton.popoverBox = this.volumeButton.popover.get_child();
        this.volumeButton.osd = this.fullscreenMode;
        this.volumeButton.connect(
            'clicked', this._onPopoverButtonClicked.bind(this, this.volumeButton.popoverBox)
        );
        let boxChildren = this.volumeButton.popoverBox.get_children();

        for(let child of boxChildren) {
            if(child.constructor === Gtk.Button) {
                this.volumeButton.popoverBox.remove(child);
                child.destroy();
            }
            else if(child.constructor === Gtk.Scale) {
                this.setDefaultWidgetBehaviour(child);
                child.height_request = 200;
                child.round_digits = 2;
                this.volumeScale = child;
                this.setVolumeMarks(true);
            }
        }
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

    _onTrackRadioButtonToggled(self, radioButton)
    {
        if(!radioButton.get_active())
            return;

        this.emit(
            'track-change-requested',
            radioButton.trackType,
            radioButton.trackId
        );
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
});
