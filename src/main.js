imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

pkg.initGettext();
pkg.initFormat();

const Debug = imports.src.debug;
Debug.debug('imports');

const { GstClapper, Gtk, Adw } = imports.gi;
const { App } = imports.src.app;

function main(argv)
{
    Debug.debug('main');

    GstClapper.Clapper.gst_init(null);
    Gtk.init();
    Adw.init();

    Debug.debug('initialized');

    new App().run(argv);
}
