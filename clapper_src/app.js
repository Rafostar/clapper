const { Gdk, GLib, GObject, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;
const { Interface } = imports.clapper_src.interface;
const { Player } = imports.clapper_src.player;
const { Window } = imports.clapper_src.window;

const APP_NAME = 'Clapper';

let { debug } = Debug;

var App = GObject.registerClass({
    Signals: {
        'ready': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
    }
}, class ClapperApp extends Gtk.Application
{
    _init(opts)
    {
        GLib.set_prgname(APP_NAME);

        super._init();

        let defaults = {
            playlist: [],
        };
        Object.assign(this, defaults, opts);

        this.window = null;
        this.interface = null;
        this.player = null;
        this.dragStartReady = false;

        this.connect('startup', () => this._buildUI());
        this.connect('activate', () => this._openWindow());
    }

    run(arr)
    {
        arr = arr || [];
        super.run(arr);
    }

    setHideCursorTimeout()
    {
        if(this.hideCursorTimeout)
            GLib.source_remove(this.hideCursorTimeout);

        this.hideCursorTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 1, () => {
            this.hideCursorTimeout = null;

            if(this.isCursorInPlayer)
                this.playerWindow.set_cursor(this.blankCursor);

            return GLib.SOURCE_REMOVE;
        });
    }

    _buildUI()
    {
        this.window = new Window(this, APP_NAME);

        this.window.connect(
            'realize', this._onWindowRealize.bind(this)
        );
        this.window.connect(
            'key-press-event', this._onWindowKeyPressEvent.bind(this)
        );
        this.window.connect(
            'fullscreen-changed', this._onWindowFullscreenChanged.bind(this)
        );

        this.interface = new Interface();
        this.interface.controls.toggleFullscreenButton.connect(
            'clicked', () => this.window.toggleFullscreen()
        );

        this.window.add(this.interface);
    }

    _openWindow()
    {
        this.window.show_all();
    }

    _onWindowRealize()
    {
        this.player = new Player();
        this.player.widget.add_events(
            Gdk.EventMask.SCROLL_MASK
            | Gdk.EventMask.ENTER_NOTIFY_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        );
        this.interface.addPlayer(this.player);

        this.player.connect('warning', this._onPlayerWarning.bind(this));
        this.player.connect('error', this._onPlayerError.bind(this));

        this.player.widget.connect(
            'button-press-event', this._onPlayerButtonPressEvent.bind(this)
        );
        this.player.widget.connect(
            'scroll-event', this._onPlayerScrollEvent.bind(this)
        );
        this.player.widget.connect(
            'enter-notify-event', this._onPlayerEnterNotifyEvent.bind(this)
        );
        this.player.widget.connect(
            'leave-notify-event', this._onPlayerLeaveNotifyEvent.bind(this)
        );
        this.player.widget.connect(
            'motion-notify-event', this._onPlayerMotionNotifyEvent.bind(this)
        );
        this.player.widget.connect(
            'realize', this._onPlayerRealize.bind(this)
        );

        if(this.playlist.length)
            this.player.set_uri(this.playlist[0]);

        this.player.widget.show_all();
        this.emit('ready', true);
    }

    _onPlayerRealize()
    {
        let display = this.player.widget.get_display();

        this.defaultCursor = Gdk.Cursor.new_from_name(
            display, 'default'
        );
        this.blankCursor = Gdk.Cursor.new_for_display(
            display, Gdk.CursorType.BLANK_CURSOR
        );

        this.playerWindow = this.player.widget.get_window();
        this.setHideCursorTimeout();
    }

    _onWindowFullscreenChanged(window, isFullscreen)
    {
        this.interface.controls.toggleFullscreenButton.image = (isFullscreen)
            ? this.interface.controls.unfullscreenImage
            : this.interface.controls.fullscreenImage;

        let action = (isFullscreen) ? 'hide' : 'show';
        this.interface.controls[action]();
    }

    _onWindowKeyPressEvent(self, event)
    {
        let [res, key] = event.get_keyval();
        if(!res) return;

        //let keyName = Gdk.keyval_name(key);
        let bool = false;

        switch(key) {
            case Gdk.KEY_space:
            case Gdk.KEY_Return:
                this.player.toggle_play();
                break;
            case Gdk.KEY_Right:
                bool = true;
            case Gdk.KEY_Left:
                //this._handleScaleIncrement('position', 'Scale', bool);
                break;
            case Gdk.KEY_Up:
                bool = true;
            case Gdk.KEY_Down:
                this._handleScaleIncrement('volume', 'Button', bool);
                break;
            case Gdk.KEY_F11:
                this.window.toggleFullscreen();
                break;
            case Gdk.KEY_Escape:
                if(this.window.isFullscreen)
                    this.window.unfullscreen();
                break;
            case Gdk.KEY_q:
            case Gdk.KEY_Q:
                this.quit();
                break;
            default:
                break;
        }
    }

    _onPlayerButtonPressEvent(self, event)
    {
        let [res, button] = event.get_button();
        if(!res) return;

        this.dragStartReady = false;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                this._handlePrimaryButtonPress(event, button);
                break;
            case Gdk.BUTTON_SECONDARY:
                if(event.get_event_type() !== Gdk.EventType.DOUBLE_BUTTON_PRESS)
                    this.player.toggle_play();
                break;
            default:
                break;
        }
    }

    _handlePrimaryButtonPress(event, button)
    {
        let eventType = event.get_event_type();

        switch(eventType) {
            case Gdk.EventType.BUTTON_PRESS:
                let [res, x, y] = event.get_root_coords();
                if(!res)
                    break;
                this.dragStartX = x;
                this.dragStartY = y;
                this.dragStartReady = true;
                break;
            case Gdk.EventType.DOUBLE_BUTTON_PRESS:
                this.window.toggleFullscreen();
                break;
            default:
                break;
        }
    }

    _onPlayerScrollEvent(self, event)
    {
        let [res, direction] = event.get_scroll_direction();
        if(!res) return;

        let type = 'volume';
        let item = 'Button';

        switch(direction) {
            case Gdk.ScrollDirection.RIGHT:
            case Gdk.ScrollDirection.LEFT:
                type = 'position';
                item = 'Scale';
            case Gdk.ScrollDirection.UP:
            case Gdk.ScrollDirection.DOWN:
                let isUp = (
                    direction === Gdk.ScrollDirection.UP
                    || direction === Gdk.ScrollDirection.RIGHT
                );
                this._handleScaleIncrement(type, item, isUp);
                break;
            default:
                break;
        }
    }

    _handleScaleIncrement(type, item, isUp)
    {
        let value = this.interface.controls[`${type}${item}`].get_value();
        let maxValue = this.interface.controls[`${type}Adjustment`].get_upper();
        let increment = this.interface.controls[`${type}Adjustment`].get_page_increment();

        value += (isUp) ? increment : -increment;
        value = (value < 0)
            ? 0
            : (value > maxValue)
            ? maxValue
            : value;

        this.interface.controls[`${type}${item}`].set_value(value);
    }

    _onPlayerEnterNotifyEvent(self, event)
    {
        this.isCursorInPlayer = true;
    }

    _onPlayerLeaveNotifyEvent(self, event)
    {
        this.isCursorInPlayer = false;
    }

    _onPlayerMotionNotifyEvent(self, event)
    {
        this.playerWindow.set_cursor(this.defaultCursor);
        this.setHideCursorTimeout();

        if(!this.dragStartReady || this.window.isFullscreen)
            return;

        let [res, x, y] = event.get_root_coords();
        if(!res) return;

        let startDrag = this.player.widget.drag_check_threshold(
            this.dragStartX, this.dragStartY, x, y
        );
        if(!startDrag) return;

        this.dragStartReady = false;
        let timestamp = event.get_time();

        this.window.begin_move_drag(
            Gdk.BUTTON_PRIMARY,
            this.dragStartX,
            this.dragStartY,
            timestamp
        );
    }

    _onPlayerWarning(self, error)
    {
        debug(error.message, 'LEVEL_WARNING');
    }

    _onPlayerError(self, error)
    {
        debug(error);
    }
});
