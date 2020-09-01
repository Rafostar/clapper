const { GObject, Gtk } = imports.gi;

var Controls = GObject.registerClass(
class ClapperControls extends Gtk.HBox
{
    _init()
    {
        super._init({
            margin_top: 4,
            margin_bottom: 4
        });

        this.togglePlayButton = Gtk.Button.new_from_icon_name(
            'media-playback-pause-symbolic',
            Gtk.IconSize.LARGE_TOOLBAR
        );
        this.pauseButton = Gtk.Button.new_from_icon_name(
            'media-playback-start-symbolic',
            Gtk.IconSize.LARGE_TOOLBAR
        );
        this.playImage = this.pauseButton.image;
        this.pauseImage = this.togglePlayButton.image;

        this.positionScale = new Gtk.Scale({
            orientation: Gtk.Orientation.HORIZONTAL,
            value_pos: Gtk.PositionType.LEFT,
            draw_value: false
        });
        this.positionAdjustment = this.positionScale.get_adjustment();

        this.volumeButton = new Gtk.ScaleButton({
            icons: ['audio-volume-muted-symbolic'],
            size: Gtk.IconSize.SMALL_TOOLBAR
        });
        this.volumeButtonImage = this.volumeButton.get_child();
        this.volumeAdjustment = this.volumeButton.get_adjustment();
        this._prepareVolumeButton();

        this.toggleFullscreenButton = Gtk.Button.new_from_icon_name(
            'view-fullscreen-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR
        );
        this.unfullscreenButton = Gtk.Button.new_from_icon_name(
            'view-restore-symbolic',
            Gtk.IconSize.SMALL_TOOLBAR
        );
        this.fullscreenImage = this.toggleFullscreenButton.image;
        this.unfullscreenImage = this.unfullscreenButton.image;

        this.pack_start(this.togglePlayButton, false, false, 4);
        this.pack_start(this.positionScale, true, true, 0);
        this.pack_start(this.volumeButton, false, false, 0);
        this.pack_start(this.toggleFullscreenButton, false, false, 4);
    }

    _prepareVolumeButton()
    {
        this.volumeAdjustment.set_upper(2);
        this.volumeAdjustment.set_step_increment(0.05);
        this.volumeAdjustment.set_page_increment(0.05);

        this.volumeButton.connect(
            'value-changed', this._onVolumeValueChanged.bind(this)
        );

        let popup = this.volumeButton.get_popup();
        let box = popup.get_child();
        let boxChildren = box.get_children();

        for(let child of boxChildren) {
            if(child.constructor === Gtk.Button)
                box.remove(child);
            else if(child.constructor === Gtk.Scale) {
                child.height_request = 200;
                child.add_mark(0, Gtk.PositionType.LEFT, '0%');
                child.add_mark(1, Gtk.PositionType.LEFT, '100%');
                child.add_mark(2, Gtk.PositionType.LEFT, '200%');
            }
        }
    }

    _onVolumeValueChanged(widget, value)
    {
        if(value <= 0)
            return;

        let iconName = (value <= 0.33)
            ? 'audio-volume-low-symbolic'
            : (value <= 0.66)
            ? 'audio-volume-medium-symbolic'
            : (value <= 1)
            ? 'audio-volume-high-symbolic'
            : 'audio-volume-overamplified-symbolic';

        if(this.volumeButtonImage.icon_name === iconName)
            return;

        this.volumeButtonImage.icon_name = iconName;
    }
});
