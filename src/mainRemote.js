imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

pkg.initGettext();

const { Gtk, Adw } = imports.gi;
const { AppRemote } = imports.src.appRemote;
const Misc = imports.src.misc;

const ID_POSTFIX = 'Remote';

Misc.clapperPath = `${pkg.datadir}/${Misc.appId}`;
Misc.appId += '.' + ID_POSTFIX;

function main(argv)
{
    Gtk.init();
    Adw.init();

    new AppRemote().run(argv);
}
