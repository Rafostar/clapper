const { Gdk, GLib, GObject, Gtk } = imports.gi;
const { Interface } = imports.clapper_src.interface;
const { Player } = imports.clapper_src.player;
const { Window } = imports.clapper_src.window;

const APP_NAME = 'Clapper';

var App = GObject.registerClass({
    Signals: {
        'ready': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
    }
}, class ClapperApp extends Gtk.Application
{
    _init(args)
    {
        GLib.set_prgname(APP_NAME);

        super._init();

        this.window = null;
        this.interface = null;
        this.player = null;

        this.connect('startup', () => this._buildUI());
        this.connect('activate', () => this._openWindow());
    }

    run(arr)
    {
        arr = arr || [];
        super.run(arr);
    }

    _buildUI()
    {
        this.window = new Window(this, APP_NAME);
        this.window.connect('realize', this._onWindowRealize.bind(this));
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
        this.player.widget.add_events(Gdk.EventMask.SCROLL_MASK);
        this.interface.addPlayer(this.player);

        this.player.connect('warning', this._onPlayerWarning.bind(this));
        this.player.connect('error', this._onPlayerError.bind(this));

        this.player.widget.connect(
            'key-press-event', this._onPlayerKeyPressEvent.bind(this)
        );
        this.player.widget.connect(
            'button-press-event', this._onPlayerButtonPressEvent.bind(this)
        );
        this.player.widget.connect(
            'scroll-event', this._onPlayerScrollEvent.bind(this)
        );

        this.player.widget.show_all();
        this.emit('ready', true);
    }

    _onWindowFullscreenChanged(window, isFullscreen)
    {
        this.interface.controls.toggleFullscreenButton.image = (isFullscreen)
            ? this.interface.controls.unfullscreenImage
            : this.interface.controls.fullscreenImage;

        let action = (isFullscreen) ? 'hide' : 'show';
        this.interface.controls[action]();
    }

    _onPlayerKeyPressEvent(self, event)
    {
        let [res, key] = event.get_keyval();
        if(!res) return;

        //let keyName = Gdk.keyval_name(key);

        switch(key) {
            case Gdk.KEY_space:
            case Gdk.KEY_Return:
                this.player.toggle_play();
                break;
            case Gdk.KEY_F11:
                this.window.toggleFullscreen();
                break;
            case Gdk.KEY_Escape:
                if(this.window.isFullscreen)
                    this.window.unfullscreen();
                break;
            default:
                break;
        }
    }

    _onPlayerButtonPressEvent(self, event)
    {
        let [res, button] = event.get_button();
        if(!res) return;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                if(event.get_event_type() === Gdk.EventType.DOUBLE_BUTTON_PRESS)
                    this.window.toggleFullscreen();
                break;
            case Gdk.BUTTON_SECONDARY:
                if(event.get_event_type() !== Gdk.EventType.DOUBLE_BUTTON_PRESS)
                    this.player.toggle_play();
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
                let value = this.interface.controls[`${type}${item}`].get_value();
                let maxValue = this.interface.controls[`${type}Adjustment`].get_upper();
                let increment = this.interface.controls[`${type}Adjustment`].get_page_increment();
                value += (
                    direction === Gdk.ScrollDirection.UP
                    || direction === Gdk.ScrollDirection.RIGHT
                )
                    ? increment
                    : -increment;
                value = (value < 0)
                    ? 0
                    : (value > maxValue)
                    ? maxValue
                    : value;
                this.interface.controls[`${type}${item}`].set_value(value);
                break;
            default:
                break;
        }
    }

    _onPlayerWarning(self, error)
    {
        log(error.message);
    }

    _onPlayerError(self, error)
    {
        logError(error);
    }
});
