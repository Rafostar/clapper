const { GLib, GObject, Gtk } = imports.gi;
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

        this.connect('startup', () => this._buildUI());
        this.connect('activate', () => this._openDialog());
    }

    run(arr)
    {
        arr = arr || [];
        super.run(arr);
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

        this.interface = new Interface();

        this.appWindow.add(this.interface);
        this.appWindow.connect('realize', this._onRealize.bind(this));
    }

    _onRealize()
    {
        this.player = new Player();
        this.interface.addPlayer(this.player);

        this.player.widget.show_all();
        this.emit('player-ready', true);
    }

    _openDialog()
    {
        this.appWindow.show_all();
    }
});
