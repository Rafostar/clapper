const { Gdk, GObject, Gtk } = imports.gi;

var Window = GObject.registerClass({
    Signals: {
        'fullscreen-changed': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
    }
}, class ClapperWindow extends Gtk.ApplicationWindow
{
    _init(application, title)
    {
        super._init({
            application: application,
            title: title || 'Clapper',
            resizable: true,
            destroy_with_parent: true,
        });
        this.isFullscreen = false;
        this.mapSignal = this.connect('map', this._onMap.bind(this));
    }

    toggleFullscreen()
    {
        let un = (this.isFullscreen) ? 'un' : '';
        this[`${un}fullscreen`]();
    }

    _onStateNotify(toplevel)
    {
        let { state } = toplevel;
        let isFullscreen = Boolean(state & Gdk.ToplevelState.FULLSCREEN);

        if(this.isFullscreen === isFullscreen)
            return;

        this.isFullscreen = isFullscreen;
        this.emit('fullscreen-changed', this.isFullscreen);
    }

    _onMap()
    {
        this.disconnect(this.mapSignal);

        let surface = this.get_surface();
        surface.connect('notify::state', this._onStateNotify.bind(this));
    }
});
