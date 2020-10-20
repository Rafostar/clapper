const { Gdk, GObject, Gtk } = imports.gi;

var Window = GObject.registerClass(
class ClapperWindow extends Gtk.ApplicationWindow
{
    _init(application, title)
    {
        super._init({
            application: application,
            title: title,
        });
    }

    updateTitlebar(mediaInfo)
    {
        
    }
});
