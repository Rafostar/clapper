imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

pkg.initGettext();

const Misc = imports.misc;
Misc.appId += '.Remote';

const { Gtk, Adw } = imports.gi;
const { AppRemote } = imports.appRemote;

function main(argv)
{
    Gtk.init();
    Adw.init();

    new AppRemote().run(argv);
}
