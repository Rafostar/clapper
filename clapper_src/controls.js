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

        this.volumeButton = new Gtk.ScaleButton({
            size: Gtk.IconSize.SMALL_TOOLBAR
        });
        this._prepareVolumeButton();

        this.pack_start(this.togglePlayButton, false, false, 4);
        this.pack_start(this.positionScale, true, true, 0);
        this.pack_start(this.volumeButton, false, false, 4);
    }

    _prepareVolumeButton()
    {
        this.volumeButtonAdjustment = this.volumeButton.get_adjustment();

        this.volumeButtonAdjustment.set_upper(2);
        this.volumeButtonAdjustment.set_step_increment(0.05);
        this.volumeButtonAdjustment.set_page_increment(0.05);

        let basicIcons = [
            "audio-volume-low-symbolic",
            "audio-volume-medium-symbolic",
            "audio-volume-medium-symbolic",
            "audio-volume-high-symbolic"
        ];

        let iconsArr = [
            "audio-volume-muted-symbolic"
        ];

        for(let icon of basicIcons)
            iconsArr = this._addManyToArr(icon, iconsArr, 5);

        iconsArr = this._addManyToArr(
             "audio-volume-overamplified-symbolic", iconsArr, 18
        );

        this.volumeButton.set_icons(iconsArr);

        let popup = this.volumeButton.get_popup();
        let box = popup.get_child();
        let boxChildren = box.get_children();

        for(let child of boxChildren) {
            if(child.constructor === Gtk.Button)
                box.remove(child);
            if(child.constructor === Gtk.Scale) {
                child.height_request = 200;
                child.add_mark(0, Gtk.PositionType.LEFT, '0%');
                child.add_mark(1, Gtk.PositionType.LEFT, '100%');
                child.add_mark(2, Gtk.PositionType.LEFT, '200%');
            }
        }
    }

    _addManyToArr(item, arr, count)
    {
        for(let i = 0; i < count; i++) {
            arr.push(item);
        }

        return arr;
    }
});
