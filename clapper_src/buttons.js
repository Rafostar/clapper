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
            can_default: false,
        });

        this.isFullscreen = isFullscreen || false;

        size = size || Gtk.IconSize.SMALL_TOOLBAR;
        let image = Gtk.Image.new_from_icon_name(icon, size);

        if(image)
            this.set_image(image);

        this.image.defaultSize = size;
        this.image.fullscreenSize = (size === Gtk.IconSize.SMALL_TOOLBAR)
            ? Gtk.IconSize.LARGE_TOOLBAR
            : Gtk.IconSize.DND;

        this.get_style_context().add_class('flat');

        this.box = new Gtk.Box();
        this.box.pack_start(this, false, false, 0);

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

var BoxedPopoverButton = GObject.registerClass(
class BoxedPopoverButton extends BoxedIconButton
{
    _init(icon, size, isFullscreen)
    {
        super._init(icon, size, isFullscreen);

        this.popover = new Gtk.Popover({
            relative_to: this.box
        });
        this.popoverBox = new Gtk.VBox();
        this.popover.add(this.popoverBox);
        this.popoverBox.show();
        this.connect(
            'clicked', this._onPopoverButtonClicked.bind(this)
        );
        if(this.isFullscreen)
            this.popover.get_style_context().add_class('osd');
    }

    setFullscreenMode(isEnabled)
    {
        if(this.isFullscreen === isEnabled)
            return;

        let action = (isEnabled) ? 'add_class' : 'remove_class';
        this.popover.get_style_context()[action]('osd');

        super.setFullscreenMode(isEnabled);
    }

    _onPopoverButtonClicked()
    {
        this.popover.popup();
    }
});
