const { Gdk, GLib, GObject, Gtk, Pango } = imports.gi;
const Debug = imports.clapper_src.debug;

const REVEAL_TIME = 800;

let { debug } = Debug;

var CustomRevealer = GObject.registerClass(
class ClapperCustomRevealer extends Gtk.Revealer
{
    _init(opts)
    {
        opts = opts || {};

        let defaults = {
            visible: false,
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
            return;

        super.set_visible(isVisible);
        debug(`${this.revealerName} revealer visible: ${isVisible}`);
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
/*
        this.set_events(
            Gdk.EventMask.BUTTON_PRESS_MASK
            | Gdk.EventMask.BUTTON_RELEASE_MASK
            | Gdk.EventMask.TOUCH_MASK
            | Gdk.EventMask.SCROLL_MASK
            | Gdk.EventMask.TOUCHPAD_GESTURE_MASK
            | Gdk.EventMask.POINTER_MOTION_MASK
            | Gdk.EventMask.ENTER_NOTIFY_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        );
*/
        let initTime = GLib.DateTime.new_now_local().format('%X');
        this.timeFormat = (initTime.length > 8)
            ? '%I:%M %p'
            : '%H:%M';

        this.revealerGrid = new Gtk.Grid({
            column_spacing: 8
        });
        this.revealerGrid.add_css_class('osd');
        this.revealerGrid.add_css_class('reavealertop');

        this.mediaTitle = new Gtk.Label({
            ellipsize: Pango.EllipsizeMode.END,
            vexpand: true,
            hexpand: true,
            margin_top: 14,
            margin_start: 12,
            xalign: 0,
            yalign: 0,
        });

        let timeLabelOpts = {
            margin_end: 10,
            xalign: 1,
            yalign: 0,
        };
        this.currentTime = new Gtk.Label(timeLabelOpts);
        this.currentTime.add_css_class('osdtime');

        this.endTime = new Gtk.Label(timeLabelOpts);
        this.endTime.add_css_class('osdendtime');

        this.revealerGrid.attach(this.mediaTitle, 0, 0, 1, 1);
        this.revealerGrid.attach(this.currentTime, 1, 0, 1, 1);
        this.revealerGrid.attach(this.endTime, 1, 0, 1, 1);

        this.set_child(this.revealerGrid);
    }

    setMediaTitle(title)
    {
        this.mediaTitle.label = title;
    }

    setTimes(currTime, endTime)
    {
        let now = currTime.format(this.timeFormat);
        let end = `Ends at: ${endTime.format(this.timeFormat)}`;

        this.currentTime.set_label(now);
        this.endTime.set_label(end);

        /* Make sure that next timeout is always run after clock changes,
         * by delaying it for additional few milliseconds */
        let nextUpdate = 60002 - parseInt(currTime.get_seconds() * 1000);
        debug(`updated current time: ${now}`);

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
});
