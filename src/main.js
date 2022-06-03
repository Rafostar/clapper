imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '3.0';

pkg.initGettext();
pkg.initFormat();

const Debug = imports.src.debug;
Debug.debug('imports');

const { GstClapper, Gtk, Adw } = imports.gi;
const { App } = imports.src.app;
const Misc = imports.src.misc;

function main(argv)
{
    Debug.debug('main');

    GstClapper.Clapper.gst_init(null);
    Gtk.init();
    Adw.init();

    /* U+2236 seems to break RTL languages, use U+003A instead */
    if(Gtk.Widget.get_default_direction() === Gtk.TextDirection.RTL)
        Misc.timeColon = ':';

    Debug.debug('initialized');

    new App().run(argv);
}
