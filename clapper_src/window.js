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
            border_width: 0,
            resizable: true,
            window_position: Gtk.WindowPosition.CENTER,
            width_request: 960,
            height_request: 642
        });
        this.isFullscreen = false;

        this.connect(
            'window-state-event', this._onWindowStateEvent.bind(this)
        );
    }

    toggleFullscreen()
    {
        let un = (this.isFullscreen) ? 'un' : '';
        this[`${un}fullscreen`]();
    }

    _onWindowStateEvent(self, event)
    {
        let window = event.get_window();
        let state = window.get_state();

        let isFullscreen = Boolean(state & Gdk.WindowState.FULLSCREEN);

        if(this.isFullscreen === isFullscreen)
            return;

        this.isFullscreen = isFullscreen;
        this.emit('fullscreen-changed', this.isFullscreen);
    }
});
