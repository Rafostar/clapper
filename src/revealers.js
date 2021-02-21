const { GLib, GObject, Gtk, Pango } = imports.gi;
const { HeaderBar } = imports.src.headerbar;
const Debug = imports.src.debug;
const DBus = imports.src.dbus;
const Misc = imports.src.misc;

const { debug } = Debug;
const { settings } = Misc;

var CustomRevealer = GObject.registerClass(
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
        if(isReveal)
            this.visible = true;

        this.reveal_child = isReveal;
    }
});

var RevealerTop = GObject.registerClass(
class ClapperRevealerTop extends CustomRevealer
{
    _init()
    {
        super._init({
            transition_type: Gtk.RevealerTransitionType.CROSSFADE,
            valign: Gtk.Align.START,
        });
        this.revealerName = 'top';

        const initTime = GLib.DateTime.new_now_local().format('%X');
        this.timeFormat = (initTime.length > 8)
            ? '%I:%M %p'
            : '%H:%M';

        this.mediaTitle = new Gtk.Label({
            ellipsize: Pango.EllipsizeMode.END,
            halign: Gtk.Align.START,
            valign: Gtk.Align.CENTER,
            hexpand: true,
            margin_start: 10,
        });
        this.mediaTitle.add_css_class('tvtitle');

        this.currentTime = new Gtk.Label({
            halign: Gtk.Align.END,
            valign: Gtk.Align.CENTER,
            margin_start: 24,
            margin_end: 10,
        });
        this.currentTime.add_css_class('tvtime');

        this.endTime = new Gtk.Label({
            margin_start: 10,
            margin_end: 10,
            margin_bottom: 2,
        });
        this.endTime.add_css_class('tvendtime');

        const revealerBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });
        this.headerBar = new HeaderBar();
        revealerBox.append(this.headerBar);

        this.revealerGrid = new Gtk.Grid({
            visible: false,
            margin_top: 8,
            margin_start: 8,
            margin_end: 8,
        });

        const topBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
        });
        topBox.add_css_class('osd');
        topBox.add_css_class('tvrevealerboxtop');
        topBox.append(this.mediaTitle);
        topBox.append(this.currentTime);

        const bottomBox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            halign: Gtk.Align.END,
        });
        bottomBox.add_css_class('osd');
        bottomBox.add_css_class('tvrevealerboxbottom');
        bottomBox.append(this.endTime);

        this.revealerGrid.attach(topBox, 0, 0, 2, 1);
        this.revealerGrid.attach(bottomBox, 1, 1, 1, 1);
        revealerBox.append(this.revealerGrid);

        this.set_child(revealerBox);
    }

    set title(text)
    {
        this.mediaTitle.label = text;
    }

    get title()
    {
        return this.mediaTitle.label;
    }

    setTimes(currTime, endTime)
    {
        const now = currTime.format(this.timeFormat);
        const end = endTime.format(this.timeFormat);
        const endText = `Ends at: ${end}`;

        this.currentTime.set_label(now);
        this.endTime.set_label(endText);

        /* Make sure that next timeout is always run after clock changes,
         * by delaying it for additional few milliseconds */
        const nextUpdate = 60002 - parseInt(currTime.get_seconds() * 1000);
        debug(`updated current time: ${now}, ends at: ${end}`);

        return nextUpdate;
    }
});

var RevealerBottom = GObject.registerClass(
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
        });
        this.revealerBox.add_css_class('osd');
        this.revealerBox.add_css_class('revealerbottombox');

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

    _onMotion(controller, x, y)
    {
        const clapperWidget = this.root.child;
        clapperWidget._clearTimeout('hideControls');
    }
});

var ControlsRevealer = GObject.registerClass(
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
    }

    _onControlsRevealed()
    {
        if(this.child_revealed) {
            const clapperWidget = this.root.child;
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

var ButtonsRevealer = GObject.registerClass(
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
