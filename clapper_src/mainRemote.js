imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { AppRemote } = imports.clapper_src.appRemote;
const Misc = imports.clapper_src.misc;

const ID_POSTFIX = 'Remote';

Misc.clapperPath = `${pkg.datadir}/${Misc.appId}`;
Misc.appId += '.' + ID_POSTFIX;

function main()
{
    new AppRemote().run();
}
