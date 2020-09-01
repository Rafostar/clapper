const { GLib, GObject, Gtk } = imports.gi;
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
        this.interface.addPlayer(this.player);

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
});
