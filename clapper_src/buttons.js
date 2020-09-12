const { GObject, Gtk } = imports.gi;

var BoxedIconButton = GObject.registerClass(
class BoxedIconButton extends Gtk.Button
{
    _init(icon, size)
    {
        super._init();

        let image = Gtk.Image.new_from_icon_name(icon, size);

        if(image)
            this.set_image(image);

        this.box = new Gtk.Box();
        this.box.pack_start(this, false, false, 0);

        super.show();
    }

    get visible()
    {
        return this.box.visible;
    }

    show_all()
    {
        this.box.show_all();
    }

    show()
    {
        this.box.show();
    }

    hide()
    {
        this.box.hide();
    }
});
