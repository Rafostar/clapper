const { GObject, Gtk } = imports.gi;

var BoxedIconButton = GObject.registerClass(
class BoxedIconButton extends Gtk.Button
{
    _init(icon, size, isFullscreen)
    {
        super._init({
            margin_top: 4,
            margin_bottom: 4,
            can_focus: false,
            //can_default: false,
        });

        this.isFullscreen = isFullscreen || false;

        size = size || Gtk.IconSize.SMALL_TOOLBAR;
        let image = Gtk.Image.new_from_icon_name(icon);

        //if(image)
            //this.set_image(image);
/*
        this.image.defaultSize = size;
        this.image.fullscreenSize = (size === Gtk.IconSize.SMALL_TOOLBAR)
            ? Gtk.IconSize.LARGE_TOOLBAR
            : Gtk.IconSize.DND;
*/
        this.add_css_class('flat');

        this.box = new Gtk.Box();
        this.box.append(this);

        super.show();
    }

    get visible()
    {
        return this.box.visible;
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        this.image.icon_size = (isFullscreen)
            ? this.image.fullscreenSize
            : this.image.defaultSize;

        this.isFullscreen = isFullscreen;
    }
/*
    show_all()
    {
        this.box.show_all();
    }
*/
    show()
    {
        this.box.show();
    }

    hide()
    {
        this.box.hide();
    }
});

var BoxedPopoverButton = GObject.registerClass(
class BoxedPopoverButton extends BoxedIconButton
{
    _init(icon, size, isFullscreen)
    {
        super._init(icon, size, isFullscreen);

        this.popover = new Gtk.Popover({
            default_widget: this.box
        });
        this.popoverBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL
        });
        this.popover.set_child(this.popoverBox);
        this.popoverBox.show();

        if(this.isFullscreen)
            this.popover.add_css_class('osd');
    }

    setFullscreenMode(isEnabled)
    {
        if(this.isFullscreen === isEnabled)
            return;

        let action = (isEnabled) ? 'add' : 'remove';
        this.popover[action + '_css_class']('osd');

        super.setFullscreenMode(isEnabled);
    }

    vfunc_clicked()
    {
        this.popover.popup();
    }
});
