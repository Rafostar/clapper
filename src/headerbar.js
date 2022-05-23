const { GObject, Gtk } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

var HeaderBar = GObject.registerClass({
    GTypeName: 'ClapperHeaderBar',
},
class ClapperHeaderBar extends Gtk.Box
{
    _init()
    {
        super._init({
            can_focus: false,
            orientation: Gtk.Orientation.HORIZONTAL,
            spacing: 6,
            margin_top: 6,
            margin_start: 6,
            margin_end: 6,
        });
        this.add_css_class('osdheaderbar');

        this.isMaximized = false;
        this.isMenuOnLeft = true;

        const uiBuilder = Misc.getBuilderForName('clapper.ui');

        this.menuWidget = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.CENTER,
            spacing: 6,
        });

        this.menuButton = new Gtk.MenuButton({
            icon_name: 'open-menu-symbolic',
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });
        const menuToggleButton = this.menuButton.get_first_child();
        menuToggleButton.add_css_class('osd');
        const mainMenuModel = uiBuilder.get_object('mainMenu');
        const mainMenuPopover = new HeaderBarPopover(mainMenuModel);
        this.menuButton.set_popover(mainMenuPopover);
        this.menuButton.add_css_class('circular');
        this.menuWidget.append(this.menuButton);

        this.extraButtonsBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.CENTER,
        });
        this.extraButtonsBox.add_css_class('linked');

        const floatButton = new Gtk.Button({
            icon_name: 'pip-in-symbolic',
            can_focus: false,
        });
        floatButton.add_css_class('osd');
        floatButton.add_css_class('circular');
        floatButton.add_css_class('linkedleft');
        floatButton.connect('clicked',
            this._onFloatButtonClicked.bind(this)
        );
        this.extraButtonsBox.append(floatButton);

        const separator = new Gtk.Separator({
            orientation: Gtk.Orientation.VERTICAL,
        });
        separator.add_css_class('linkseparator');
        this.extraButtonsBox.append(separator);

        const fullscreenButton = new Gtk.Button({
            icon_name: 'view-fullscreen-symbolic',
            can_focus: false,
        });
        fullscreenButton.add_css_class('osd');
        fullscreenButton.add_css_class('circular');
        fullscreenButton.add_css_class('linkedright');
        fullscreenButton.connect('clicked',
            this._onFullscreenButtonClicked.bind(this)
        );
        this.extraButtonsBox.append(fullscreenButton);
        this.menuWidget.append(this.extraButtonsBox);

        this.spacerWidget = new Gtk.Box({
            hexpand: true,
        });

        this.minimizeWidget = this._getWindowButton('minimize');
        this.maximizeWidget = this._getWindowButton('maximize');
        this.closeWidget = this._getWindowButton('close');

        const gtkSettings = Gtk.Settings.get_default();
        this._onLayoutUpdate(gtkSettings);

        gtkSettings.connect(
            'notify::gtk-decoration-layout',
            this._onLayoutUpdate.bind(this)
        );
    }

    setMenuOnLeft(isOnLeft)
    {
        if(this.isMenuOnLeft === isOnLeft)
            return;

        if(isOnLeft) {
            this.menuWidget.reorder_child_after(
                this.extraButtonsBox, this.menuButton
            );
        }
        else {
            this.menuWidget.reorder_child_after(
                this.menuButton, this.extraButtonsBox
            );
        }

        this.isMenuOnLeft = isOnLeft;
    }

    setMaximized(isMaximized)
    {
        if(this.isMaximized === isMaximized)
            return;

        this.maximizeWidget.icon_name = (isMaximized)
            ? 'window-restore-symbolic'
            : 'window-maximize-symbolic';

        this.isMaximized = isMaximized;
    }

    _onLayoutUpdate(gtkSettings)
    {
        const gtkLayout = gtkSettings.gtk_decoration_layout;

        this._replaceButtons(gtkLayout);
    }

    _replaceButtons(gtkLayout)
    {
        const modLayout = gtkLayout.replace(':', ',spacer,');
        const layoutArr = modLayout.split(',');

        let lastWidget = null;

        let showMinimize = false;
        let showMaximize = false;
        let showClose = false;

        let menuAdded = false;
        let spacerAdded = false;

        debug(`headerbar layout: ${modLayout}`);

        for(let name of layoutArr) {
            /* Menu might be named "appmenu" */
            if(!menuAdded && (!name || name === 'appmenu' || name === 'icon'))
                name = 'menu';

            const widget = this[`${name}Widget`];
            if(!widget) continue;

            if(!widget.parent)
                this.append(widget);
            else
                this.reorder_child_after(widget, lastWidget);

            switch(name) {
                case 'spacer':
                    spacerAdded = true;
                    break;
                case 'minimize':
                    showMinimize = true;
                    break;
                case 'maximize':
                    showMaximize = true;
                    break;
                case 'close':
                    showClose = true;
                    break;
                case 'menu':
                    this.setMenuOnLeft(!spacerAdded);
                    menuAdded = true;
                    break;
                default:
                    break;
            }

            lastWidget = widget;
        }

        this.minimizeWidget.visible = showMinimize;
        this.maximizeWidget.visible = showMaximize;
        this.closeWidget.visible = showClose;
    }

    _getWindowButton(name)
    {
        const button = new Gtk.Button({
            icon_name: `window-${name}-symbolic`,
            valign: Gtk.Align.CENTER,
            can_focus: false,
        });
        button.add_css_class('osd');
        button.add_css_class('circular');

        if(name === 'maximize')
            name = 'toggle-maximized';

        button.connect('clicked',
            this._onWindowButtonActivate.bind(this, name)
        );

        return button;
    }

    _updateFloatIcon(isFloating)
    {
        const floatButton = this.extraButtonsBox.get_first_child();
        if(!floatButton) return;

        const iconName = (isFloating)
            ? 'pip-out-symbolic'
            : 'pip-in-symbolic';

        if(floatButton.icon_name !== iconName)
            floatButton.icon_name = iconName;
    }

    _onWindowButtonActivate(action)
    {
        this.activate_action(`window.${action}`, null);
    }

    _onFloatButtonClicked(button)
    {
        const clapperWidget = this.root.child;
        const { controlsRevealer } = clapperWidget;

        controlsRevealer.toggleReveal();

        /* Reset timer to not disappear during click */
        clapperWidget._setHideControlsTimeout();

        this._updateFloatIcon(!controlsRevealer.reveal_child);
    }

    _onFullscreenButtonClicked(button)
    {
        this.root.fullscreen();
    }
});

var HeaderBarPopover = GObject.registerClass({
    GTypeName: 'ClapperHeaderBarPopover',
},
class ClapperHeaderBarPopover extends Gtk.PopoverMenu
{
    _init(model)
    {
        super._init({
            menu_model: model,
        });

        this.connect('map', this._onMap.bind(this));
        this.connect('closed', this._onClosed.bind(this));
    }

    _onMap()
    {
        const { child } = this.root;

        if(
            !child
            || !child.player
            || !child.player.widget
        )
            return;

        child.revealControls();
        child.isPopoverOpen = true;
    }

    _onClosed()
    {
        const { child } = this.root;

        if(
            !child
            || !child.player
            || !child.player.widget
        )
            return;

        child.revealControls();
        child.isPopoverOpen = false;
    }
});
