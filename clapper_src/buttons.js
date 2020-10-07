const { GObject, Gtk } = imports.gi;

var IconButton = GObject.registerClass(
class ClapperIconButton extends Gtk.Button
{
    _init(icon)
    {
        super._init({
            margin_top: 4,
            margin_bottom: 4,
            margin_start: 1,
            margin_end: 1,
            can_focus: false,
            icon_name: icon,
        });

        this.isFullscreen = false;
        this.add_css_class('flat');
    }

    setFullscreenMode(isFullscreen)
    {
        this.isFullscreen = isFullscreen;
    }
});

var PopoverButton = GObject.registerClass(
class ClapperPopoverButton extends IconButton
{
    _init(icon)
    {
        super._init(icon);

        this.popover = new Gtk.Popover({
            position: Gtk.PositionType.TOP,
        });
        this.popoverBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });

        this.popover.set_parent(this);
        this.popover.set_child(this.popoverBox);
        this.popover.set_offset(0, -this.margin_top);

        if(this.isFullscreen)
            this.popover.add_css_class('osd');

        this.destroySignal = this.connect('destroy', this._onDestroy.bind(this));
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        this.margin_top = (isFullscreen) ? 6 : 4;
        this.popover.set_offset(0, -this.margin_top);

        let cssClass = 'osd';
        if(isFullscreen == this.popover.has_css_class(cssClass))
            return;

        let action = (isFullscreen) ? 'add' : 'remove';
        this.popover[action + '_css_class'](cssClass);

        super.setFullscreenMode(isFullscreen);
    }

    vfunc_clicked()
    {
        this.popover.popup();
    }

    _onDestroy()
    {
        this.disconnect(this.destroySignal);

        this.popover.unparent();
        this.popoverBox.emit('destroy');
        this.popover.emit('destroy');
    }
});
