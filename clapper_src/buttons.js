const { GObject, Gtk } = imports.gi;

var CustomButton = GObject.registerClass(
class ClapperCustomButton extends Gtk.Button
{
    _init(opts)
    {
        opts = opts || {};

        const defaults = {
            margin_top: 4,
            margin_bottom: 4,
            margin_start: 2,
            margin_end: 2,
            can_focus: false,
        };
        Object.assign(opts, defaults);

        super._init(opts);

        this.floatUnaffected = false;
        this.wantedVisible = true;
        this.isFullscreen = false;
        this.isFloating = false;

        this.add_css_class('flat');
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        this.margin_top = (isFullscreen) ? 5 : 4;
        this.margin_start = (isFullscreen) ? 3 : 2;
        this.margin_end = (isFullscreen) ? 3 : 2;
        this.can_focus = isFullscreen;

        this.isFullscreen = isFullscreen;
    }

    setFloatingMode(isFloating)
    {
        if(this.isFloating === isFloating)
            return;

        this.isFloating = isFloating;

        if(this.floatUnaffected)
            return;

        if(isFloating)
            super.set_visible(false);
        else
            super.set_visible(this.wantedVisible);
    }

    set_visible(isVisible)
    {
        this.wantedVisible = isVisible;

        if(this.isFloating && !this.floatUnaffected)
            super.set_visible(false);
        else
            super.set_visible(isVisible);
    }

    vfunc_clicked()
    {
        if(!this.isFullscreen)
            return;

        const { player } = this.get_ancestor(Gtk.Grid);
        player._setHideControlsTimeout();
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
        this.floatUnaffected = true;
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

var PopoverButtonBase = GObject.registerClass(
class ClapperPopoverButtonBase extends CustomButton
{
    _init()
    {
        super._init();

        this.popover = new Gtk.Popover({
            position: Gtk.PositionType.TOP,
        });
        this.popoverBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });

        this.popover.set_child(this.popoverBox);
        this.popover.set_offset(0, -this.margin_top);

        if(this.isFullscreen)
            this.popover.add_css_class('osd');

        this.popover.connect('closed', this._onClosed.bind(this));
        this.popover.set_parent(this);
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        super.setFullscreenMode(isFullscreen);

        this.popover.set_offset(0, -this.margin_top);

        const cssClass = 'osd';
        if(isFullscreen === this.popover.has_css_class(cssClass))
            return;

        const action = (isFullscreen) ? 'add' : 'remove';
        this.popover[action + '_css_class'](cssClass);
    }

    vfunc_clicked()
    {
        super.vfunc_clicked();

        this.set_state_flags(Gtk.StateFlags.CHECKED, false);
        this.popover.popup();
    }

    _onClosed()
    {
        const { player } = this.get_ancestor(Gtk.Grid);
        player.widget.grab_focus();

        this.unset_state_flags(Gtk.StateFlags.CHECKED);
    }

    _onCloseRequest()
    {
        this.popover.unparent();
    }
});

var IconPopoverButton = GObject.registerClass(
class ClapperIconPopoverButton extends PopoverButtonBase
{
    _init(icon)
    {
        super._init();

        this.icon_name = icon;
    }
});

var LabelPopoverButton = GObject.registerClass(
class ClapperLabelPopoverButton extends PopoverButtonBase
{
    _init(text)
    {
        super._init();

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

var ElapsedPopoverButton = GObject.registerClass(
class ClapperElapsedPopoverButton extends LabelPopoverButton
{
    _init(text)
    {
        super._init(text);

        this.scrolledWindow = new Gtk.ScrolledWindow({
            max_content_height: 150,
            min_content_width: 250,
            propagate_natural_height: true,
        });
        this.popoverBox.append(this.scrolledWindow);
    }

    setFullscreenMode(isFullscreen)
    {
        super.setFullscreenMode(isFullscreen);

        this.scrolledWindow.max_content_height = (isFullscreen)
            ? 190 : 150;
    }
});
