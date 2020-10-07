const { GObject, Gtk } = imports.gi;

var CustomButton = GObject.registerClass(
class ClapperCustomButton extends Gtk.Button
{
    _init(opts)
    {
        opts = opts || {};

        let defaults = {
            margin_top: 4,
            margin_bottom: 4,
            margin_start: 1,
            margin_end: 1,
            can_focus: false,
        };
        Object.assign(opts, defaults);

        super._init(opts);

        this.isFullscreen = false;
        this.add_css_class('flat');
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        this.margin_top = (isFullscreen) ? 6 : 4;
        this.isFullscreen = isFullscreen;
    }
});

var IconButton = GObject.registerClass(
class ClapperIconButton extends CustomButton
{
    _init(icon)
    {
        super._init({
            icon_name: icon,
        });
    }
});

var IconToggleButton = GObject.registerClass(
class ClapperIconToggleButton extends IconButton
{
    _init(primaryIcon, secondaryIcon)
    {
        super._init(primaryIcon);

        this.primaryIcon = primaryIcon;
        this.secondaryIcon = secondaryIcon;
    }

    setPrimaryIcon()
    {
        this.icon_name = this.primaryIcon;
    }

    setSecondaryIcon()
    {
        this.icon_name = this.secondaryIcon;
    }
});

var LabelButton = GObject.registerClass(
class ClapperLabelButton extends CustomButton
{
    _init(text)
    {
        super._init({
            margin_start: 0,
            margin_end: 0,
        });

        this.customLabel = new Gtk.Label({
            label: text,
            single_line_mode: true,
        });

        this.customLabel.add_css_class('labelbutton');
        this.set_child(this.customLabel);
    }

    set_label(text)
    {
        this.customLabel.set_text(text);
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

        super.setFullscreenMode(isFullscreen);

        this.popover.set_offset(0, -this.margin_top);

        let cssClass = 'osd';
        if(isFullscreen == this.popover.has_css_class(cssClass))
            return;

        let action = (isFullscreen) ? 'add' : 'remove';
        this.popover[action + '_css_class'](cssClass);
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
