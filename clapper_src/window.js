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
    }

    toggleFullscreen()
    {
        let un = (this.isFullscreen) ? 'un' : '';
        this[`${un}fullscreen`]();
    }

    vfunc_window_state_event(event)
    {
        super.vfunc_window_state_event(event);

        let isFullscreen = Boolean(
            event.new_window_state
            & Gdk.WindowState.FULLSCREEN
        );

        if(this.isFullscreen === isFullscreen)
            return;

        this.isFullscreen = isFullscreen;
        this.emit('fullscreen-changed', this.isFullscreen);
    }
});
