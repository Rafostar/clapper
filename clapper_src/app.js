const { Gdk, GLib, GObject, Gtk } = imports.gi;
const { Player } = imports.clapper_src.player;
const { Interface } = imports.clapper_src.interface;

const APP_NAME = 'Clapper';

var App = GObject.registerClass({
    Signals: {
        'player-ready': {
            param_types: [GObject.TYPE_BOOLEAN]
        }
    }
}, class ClapperApp extends Gtk.Application
{
    _init(args)
    {
        GLib.set_prgname(APP_NAME);

        super._init();

        this.isFullscreen = false;
        this.connect('startup', () => this._buildUI());
        this.connect('activate', () => this._openDialog());
    }

    run(arr)
    {
        arr = arr || [];
        super.run(arr);
    }

    toggleFullscreen()
    {
        let isUn = (this.isFullscreen) ? 'un' : '';
        this.appWindow[`${isUn}fullscreen`]();
    }

    _buildUI()
    {
        this.appWindow = new Gtk.ApplicationWindow({
            application: this,
            title: APP_NAME,
            border_width: 0,
            resizable: true,
            window_position: Gtk.WindowPosition.CENTER,
            width_request: 960,
            height_request: 642
        });
        this.appWindow.connect(
            'window-state-event', this._onWindowStateEvent.bind(this)
        );

        this.interface = new Interface();
        this.interface.controls.toggleFullscreenButton.connect(
            'clicked', this.toggleFullscreen.bind(this)
        );

        this.appWindow.add(this.interface);
        this.appWindow.connect('realize', this._onRealize.bind(this));
    }

    _openDialog()
    {
        this.appWindow.show_all();
    }

    _onRealize()
    {
        this.player = new Player();
        this.interface.addPlayer(this.player);

        this.player.widget.show_all();
        this.emit('player-ready', true);
    }

    _onWindowStateEvent(widget, event)
    {
        let window = event.get_window();
        let state = window.get_state();

        this.isFullscreen = Boolean(state & Gdk.WindowState.FULLSCREEN);
        this.interface.controls.toggleFullscreenButton.image = (this.isFullscreen)
            ? this.interface.controls.unfullscreenImage
            : this.interface.controls.fullscreenImage;

        let action = (this.isFullscreen) ? 'hide' : 'show';
        this.interface.controls[action]();
    }
});
