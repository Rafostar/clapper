imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

pkg.initGettext();

const { GstClapper, Gtk, Adw } = imports.gi;
const { App } = imports.src.app;

function main(argv)
{
    GstClapper.Clapper.gst_init(null);
    Gtk.init();
    Adw.init();

    new App().run(argv);
}
