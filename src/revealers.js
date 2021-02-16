const { GLib, GObject, Gtk, Pango } = imports.gi;
const { HeaderBar } = imports.src.headerbar;
const Debug = imports.src.debug;
const DBus = imports.src.dbus;
const Misc = imports.src.misc;

const REVEAL_TIME = 800;

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
        };
        Object.assign(opts, defaults);

        super._init(opts);

        this.revealerName = '';
    }

    revealChild(isReveal)
    {
        if(isReveal) {
            this._clearTimeout();
            this.set_visible(isReveal);
        }
        else
            this._setHideTimeout();

        /* Restore focusability after we are done */
        if(!isReveal) this.set_can_focus(true);

        this._timedReveal(isReveal, REVEAL_TIME);
    }

    showChild(isReveal)
    {
        this._clearTimeout();
        this.set_visible(isReveal);
        this._timedReveal(isReveal, 0);
    }

    set_visible(isVisible)
    {
        if(this.visible === isVisible)
            return false;

        super.set_visible(isVisible);
        debug(`${this.revealerName} revealer visible: ${isVisible}`);

        return true;
    }

    _timedReveal(isReveal, time)
    {
        this.set_transition_duration(time);
        this.set_reveal_child(isReveal);
    }

    /* Drawing revealers on top of video frames
     * increases CPU usage, so we hide them */
    _setHideTimeout()
    {
        this._clearTimeout();

        this._revealerTimeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, REVEAL_TIME + 20, () => {
            this._revealerTimeout = null;
            this.set_visible(false);

            return GLib.SOURCE_REMOVE;
        });
    }

    _clearTimeout()
    {
        if(!this._revealerTimeout)
            return;

        GLib.source_remove(this._revealerTimeout);
        this._revealerTimeout = null;
    }
});

var RevealerTop = GObject.registerClass(
class ClapperRevealerTop extends CustomRevealer
{
    _init()
    {
        super._init({
            transition_duration: REVEAL_TIME,
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
            vexpand: true,
            hexpand: true,
            margin_top: 14,
            margin_start: 12,
            xalign: 0,
            yalign: 0,
        });

        const timeLabelOpts = {
            margin_end: 10,
            xalign: 1,
            yalign: 0,
        };
        this.currentTime = new Gtk.Label(timeLabelOpts);
        this.currentTime.add_css_class('tvtime');

        timeLabelOpts.visible = false;
        this.endTime = new Gtk.Label(timeLabelOpts);
        this.endTime.add_css_class('tvendtime');

        const revealerBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });
        revealerBox.add_css_class('osd');
        revealerBox.add_css_class('reavealertop');

        this.headerBar = new HeaderBar();
        revealerBox.append(this.headerBar);

        this.revealerGrid = new Gtk.Grid({
            column_spacing: 8,
            visible: false,
        });
        this.revealerGrid.attach(this.mediaTitle, 0, 0, 1, 1);
        this.revealerGrid.attach(this.currentTime, 1, 0, 1, 1);
        this.revealerGrid.attach(this.endTime, 1, 0, 1, 1);
        revealerBox.append(this.revealerGrid);

        this.set_child(revealerBox);
    }

    setMediaTitle(title)
    {
        this.mediaTitle.label = title;
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
            transition_duration: REVEAL_TIME,
            transition_type: Gtk.RevealerTransitionType.SLIDE_UP,
            valign: Gtk.Align.END,
        });

        this.revealerName = 'bottom';
        this.revealerBox = new Gtk.Box();
        this.revealerBox.add_css_class('osd');

        this.set_child(this.revealerBox);
    }

    append(widget)
    {
        this.revealerBox.append(widget);
    }

    remove(widget)
    {
        this.revealerBox.remove(widget);
    }

    set_visible(isVisible)
    {
        const isChange = super.set_visible(isVisible);
        if(!isChange || !this.can_focus) return;

        const parent = this.get_parent();
        const playerWidget = parent.get_first_child();
        if(!playerWidget) return;

        if(isVisible) {
            const box = this.get_first_child();
            if(!box) return;

            const controls = box.get_first_child();
            if(!controls) return;

            const togglePlayButton = controls.get_first_child();
            if(togglePlayButton) {
                togglePlayButton.grab_focus();
                debug('focus moved to toggle play button');
            }
            playerWidget.set_can_focus(false);
        }
        else {
            playerWidget.set_can_focus(true);
            playerWidget.grab_focus();
            debug('focus moved to player widget');
        }
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
