const { GLib, GObject, Gtk, Pango } = imports.gi;
const { HeaderBar } = imports.src.headerbar;
const Debug = imports.src.debug;
const DBus = imports.src.dbus;
const Misc = imports.src.misc;

const { debug } = Debug;
const { settings } = Misc;

var CustomRevealer = GObject.registerClass({
    GTypeName: 'ClapperCustomRevealer',
},
class ClapperCustomRevealer extends Gtk.Revealer
{
    _init(opts)
    {
        opts = opts || {};

        const defaults = {
            visible: false,
            can_focus: false,
            transition_duration: 800,
        };
        Object.assign(opts, defaults);

        super._init(opts);

        this.revealerName = '';
        this.bind_property('child_revealed', this, 'visible',
            GObject.BindingFlags.DEFAULT
        );
    }

    revealChild(isReveal)
    {
        if(this.reveal_child === isReveal)
            return;

        if(isReveal)
            this.visible = true;

        this.reveal_child = isReveal;
    }
});

var RevealerTop = GObject.registerClass({
    GTypeName: 'ClapperRevealerTop',
},
class ClapperRevealerTop extends CustomRevealer
{
    _init()
    {
        super._init({
            transition_type: Gtk.RevealerTransitionType.CROSSFADE,
            valign: Gtk.Align.START,
        });
        this.revealerName = 'top';
        this._requestedTransition = this.transition_type;

        const initTime = GLib.DateTime.new_now_local().format('%X');
        this.timeFormat = (initTime.length > 8)
            ? `%I${Misc.timeColon}%M %p`
            : `%H${Misc.timeColon}%M`;

        this.mediaTitle = new Gtk.Label({
            ellipsize: Pango.EllipsizeMode.END,
            halign: Gtk.Align.START,
            valign: Gtk.Align.CENTER,
            margin_start: 10,
            margin_end: 10,
            visible: false,
        });
        this.mediaTitle.add_css_class('tvtitle');

        this.currentTime = new Gtk.Label({
            halign: Gtk.Align.END,
            valign: Gtk.Align.CENTER,
            margin_start: 10,
            margin_end: 10,
        });
        this.currentTime.add_css_class('tvtime');

        this.endTime = new Gtk.Label({
            halign: Gtk.Align.END,
            valign: Gtk.Align.START,
            margin_start: 10,
            margin_end: 10,
            visible: false,
        });
        this.endTime.add_css_class('tvendtime');

        const revealerBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });
        this.headerBar = new HeaderBar();
        revealerBox.append(this.headerBar);

        this.revealerGrid = new Gtk.Grid({
            column_spacing: 4,
            margin_top: 8,
            margin_start: 8,
            margin_end: 8,
            visible: false,
        });
        this.revealerGrid.add_css_class('revealertopgrid');

        const topLeftBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
        });
        topLeftBox.add_css_class('osd');
        topLeftBox.add_css_class('roundedcorners');
        topLeftBox.append(this.mediaTitle);

        const topSpacerBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            hexpand: true,
        });

        const topRightBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            halign: Gtk.Align.END,
        });
        topRightBox.add_css_class('osd');
        topRightBox.add_css_class('roundedcorners');
        topRightBox.append(this.currentTime);
        topRightBox.append(this.endTime);

        this.revealerGrid.attach(topLeftBox, 0, 0, 1, 1);
        this.revealerGrid.attach(topSpacerBox, 1, 0, 1, 1);
        this.revealerGrid.attach(topRightBox, 2, 0, 1, 2);
        revealerBox.append(this.revealerGrid);

        this.set_child(revealerBox);

        this.mediaTitle.bind_property('visible', this.endTime, 'visible',
            GObject.BindingFlags.DEFAULT
        );
        this.connect('notify::child-revealed', this._onTopRevealed.bind(this));
    }

    set title(text)
    {
        this.mediaTitle.label = text;
    }

    get title()
    {
        return this.mediaTitle.label;
    }

    set showTitle(isShow)
    {
        this.mediaTitle.visible = isShow;
    }

    get showTitle()
    {
        return this.mediaTitle.visible;
    }

    setTimes(currTime, endTime, isEndKnown)
    {
        const now = currTime.format(this.timeFormat);
        this.currentTime.label = now;

        const end = (isEndKnown)
            ? endTime.format(this.timeFormat)
            : 'unknown';

        this.endTime.label = _('Ends at: %s').format(end);

        /* Make sure that next timeout is always run after clock changes,
         * by delaying it for additional few milliseconds */
        const nextUpdate = 60004 - parseInt(currTime.get_seconds() * 1000);
        debug(`updated current time: ${now}, ends at: ${end}`);

        return nextUpdate;
    }

    setFullscreenMode(isFullscreen, isMobileMonitor)
    {
        const isTvMode = (isFullscreen && !isMobileMonitor);

        this.headerBar.visible = !isTvMode;
        this.revealerGrid.visible = isTvMode;

        this.headerBar.extraButtonsBox.visible = !isFullscreen;

        this._requestedTransition = (isTvMode)
            ? Gtk.RevealerTransitionType.SLIDE_DOWN
            : Gtk.RevealerTransitionType.CROSSFADE;

        const isRevealed = this.child_revealed;

        /* FIXME: Changing transition in middle or when not fully
         * revealed has dire consequences, seems to be a GTK4 bug */
        if(isRevealed && isRevealed === this.reveal_child)
            this._checkSwitchTransitionType();
    }

    _checkSwitchTransitionType()
    {
        if(this.transition_type !== this._requestedTransition)
            this.transition_type = this._requestedTransition;
    }

    _onTopRevealed()
    {
        if(this.child_revealed) {
            /* TODO: Move before above if statement when GTK4 can handle
             * changing transition type while not fully revealed */
            this._checkSwitchTransitionType();

            const clapperWidget = this.root.child;
            if(!clapperWidget) return;

            clapperWidget._setHideControlsTimeout();
        }
    }
});

var RevealerBottom = GObject.registerClass({
    GTypeName: 'ClapperRevealerBottom',
},
class ClapperRevealerBottom extends CustomRevealer
{
    _init()
    {
        super._init({
            transition_type: Gtk.RevealerTransitionType.SLIDE_UP,
            valign: Gtk.Align.END,
        });

        this.revealerName = 'bottom';
        this.revealerBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            margin_start: 8,
            margin_end: 8,
            margin_bottom: 8,
            visible: false,
        });
        this.revealerBox.add_css_class('osd');
        this.revealerBox.add_css_class('roundedcorners');

        this.set_child(this.revealerBox);

        const motionController = new Gtk.EventControllerMotion();
        motionController.connect('motion', this._onMotion.bind(this));
        this.add_controller(motionController);
    }

    append(widget)
    {
        this.revealerBox.append(widget);
    }

    remove(widget)
    {
        this.revealerBox.remove(widget);
    }

    setLayoutMargins(layoutWidth)
    {
        const maxWidth = 1720;

        const margin = (layoutWidth > maxWidth)
            ? (layoutWidth - maxWidth) / 2
            : 0;

        this.margin_start = margin;
        this.margin_end = margin;
    }

    _onMotion(controller, x, y)
    {
        const clapperWidget = this.root.child;
        clapperWidget._clearTimeout('hideControls');
    }
});

var ControlsRevealer = GObject.registerClass({
    GTypeName: 'ClapperControlsRevealer',
},
class ClapperControlsRevealer extends Gtk.Revealer
{
    _init()
    {
        super._init({
            transition_duration: 600,
            transition_type: Gtk.RevealerTransitionType.SLIDE_DOWN,
            reveal_child: true,
        });

        this.connect('notify::child-revealed', this._onControlsRevealed.bind(this));
    }

    toggleReveal()
    {
        /* Prevent interrupting transition */
        if(this.reveal_child !== this.child_revealed)
            return;

        const { widget } = this.root.child.player;

        if(this.child_revealed) {
            const [width] = this.root.get_default_size();
            const height = widget.get_height();

            this.add_tick_callback(
                this._onUnrevealTick.bind(this, widget, width, height)
            );
        }
        else
            this.visible = true;

        widget.height_request = widget.get_height();
        this.reveal_child ^= true;

        const isFloating = !this.reveal_child;
        DBus.shellWindowEval('make_above', isFloating);

        const isStick = (isFloating && settings.get_boolean('floating-stick'));
        DBus.shellWindowEval('stick', isStick);

        this.root.child.refreshWindowTitle(this.root.title);
    }

    _onControlsRevealed()
    {
        if(this.child_revealed) {
            const clapperWidget = this.root.child;
            if(!clapperWidget) return;

            const [width, height] = this.root.get_default_size();

            clapperWidget.player.widget.height_request = -1;
            this.root.set_default_size(width, height);
        }
    }

    _onUnrevealTick(playerWidget, width, height)
    {
        const isRevealed = this.child_revealed;

        if(!isRevealed) {
            playerWidget.height_request = -1;
            this.visible = false;
        }
        this.root.set_default_size(width, height);

        return isRevealed;
    }
});

var ButtonsRevealer = GObject.registerClass({
    GTypeName: 'ClapperButtonsRevealer',
},
class ClapperButtonsRevealer extends Gtk.Revealer
{
    _init(trType, toggleButton)
    {
        super._init({
            transition_duration: 500,
            transition_type: Gtk.RevealerTransitionType[trType],
        });

        const revealerBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
        });
        this.set_child(revealerBox);

        if(toggleButton) {
            toggleButton.connect('clicked', this._onToggleButtonClicked.bind(this));
            this.connect('notify::reveal-child', this._onRevealChild.bind(this, toggleButton));
            this.connect('notify::child-revealed', this._onChildRevealed.bind(this, toggleButton));
        }
    }

    append(widget)
    {
        this.get_child().append(widget);
    }

    revealInstantly(isReveal)
    {
        if(this.child_revealed === isReveal)
            return;

        const initialDuration = this.transition_duration;

        this.transition_duration = 0;
        this.reveal_child = isReveal;
        this.transition_duration = initialDuration;
    }

    _setRotateClass(icon, isAdd)
    {
        const cssClass = 'halfrotate';
        const hasClass = icon.has_css_class(cssClass);

        if(!hasClass && isAdd)
            icon.add_css_class(cssClass);
        else if(hasClass && !isAdd)
            icon.remove_css_class(cssClass);
    }

    _onToggleButtonClicked(button)
    {
        this.set_reveal_child(!this.reveal_child);
    }

    _onRevealChild(button)
    {
        if(this.reveal_child !== this.child_revealed)
            this._setRotateClass(button.child, true);
    }

    _onChildRevealed(button)
    {
        if(!this.child_revealed)
            button.setPrimaryIcon();
        else
            button.setSecondaryIcon();

        this._setRotateClass(button.child, false);
    }
});
