imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

const { Gtk, Adw } = imports.gi;

Gtk.init();
Adw.init();

const { AppRemote } = imports.src.appRemote;
const Misc = imports.src.misc;

const ID_POSTFIX = 'Remote';

Misc.clapperPath = `${pkg.datadir}/${Misc.appId}`;
Misc.appId += '.' + ID_POSTFIX;

function main(argv)
{
    new AppRemote().run(argv);
}
