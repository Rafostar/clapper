imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

const { GstClapper, Gtk, Adw } = imports.gi;

GstClapper.Clapper.gst_init(null);
Gtk.init();
Adw.init();

const { App } = imports.src.app;

function main(argv)
{
    new App().run(argv);
}
