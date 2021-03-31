const { GObject, Gtk } = imports.gi;

/* Negative values from CSS */
const PopoverOffset = {
  DEFAULT: -3,
  TVMODE: -5,
};

var CustomButton = GObject.registerClass(
class ClapperCustomButton extends Gtk.Button
{
    _init(opts)
    {
        opts = opts || {};

        const defaults = {
            halign: Gtk.Align.CENTER,
            valign: Gtk.Align.CENTER,
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

        this.can_focus = isFullscreen;

        /* Redraw icon after style class change */
        if(this.icon_name)
            this.set_icon_name(this.icon_name);

        this.isFullscreen = isFullscreen;
    }

    vfunc_clicked()
    {
        if(!this.isFullscreen)
            return;

        const clapperWidget = this.get_ancestor(Gtk.Grid);
        clapperWidget.revealControls();
    }
});

var IconToggleButton = GObject.registerClass(
class ClapperIconToggleButton extends CustomButton
{
    _init(primaryIcon, secondaryIcon)
    {
        super._init({
            icon_name: primaryIcon,
        });

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
class ClapperPopoverButtonBase extends Gtk.ToggleButton
{
    _init()
    {
        super._init({
            halign: Gtk.Align.CENTER,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });

        this.isFullscreen = false;
        this.add_css_class('flat');

        this.popover = new Gtk.Popover({
            position: Gtk.PositionType.TOP,
        });
        this.popoverBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });

        this.popover.set_child(this.popoverBox);
        this.popover.set_offset(0, PopoverOffset.DEFAULT);

        if(this.isFullscreen)
            this.popover.add_css_class('osd');

        this.popover.connect('closed', this._onClosed.bind(this));
        this.popover.set_parent(this);
    }

    setFullscreenMode(isFullscreen)
    {
        if(this.isFullscreen === isFullscreen)
            return;

        this.can_focus = isFullscreen;

        /* Redraw icon after style class change */
        if(this.icon_name)
            this.set_icon_name(this.icon_name);

        this.isFullscreen = isFullscreen;

        /* TODO: Fullscreen non-tv mode */
        const offset = (isFullscreen)
            ? PopoverOffset.TVMODE
            : PopoverOffset.DEFAULT;

        this.popover.set_offset(0, offset);

        const cssClass = 'osd';
        if(isFullscreen === this.popover.has_css_class(cssClass))
            return;

        const action = (isFullscreen) ? 'add' : 'remove';
        this.popover[action + '_css_class'](cssClass);
    }

    vfunc_toggled()
    {
        if(!this.active)
            return;

        const clapperWidget = this.get_ancestor(Gtk.Grid);

        if(this.isFullscreen) {
            clapperWidget.revealControls();
            clapperWidget.isPopoverOpen = true;
        }

        this.popover.popup();
    }

    _onClosed()
    {
        const clapperWidget = this.get_ancestor(Gtk.Grid);

        clapperWidget.player.widget.grab_focus();

        /* Set again timeout as popover is now closed */
        if(clapperWidget.isFullscreenMode)
            clapperWidget.revealControls();

        clapperWidget.isPopoverOpen = false;
        this.active = false;
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
        this.customLabel.add_css_class('labelbuttonlabel');
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

        this.popoverBox.add_css_class('elapsedpopoverbox');

        this.scrolledWindow = new Gtk.ScrolledWindow({
            max_content_height: 150,
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

    addSeparator(text)
    {
        const box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
        });
        const label = new Gtk.Label({
            label: text,
            halign: Gtk.Align.CENTER,
        });
        const leftSeparator = new Gtk.Separator({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
            valign: Gtk.Align.CENTER,
        });
        const rightSeparator = new Gtk.Separator({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
            valign: Gtk.Align.CENTER,
        });
        box.append(leftSeparator);
        box.append(label);
        box.append(rightSeparator);
        this.popoverBox.append(box);
    }
});
