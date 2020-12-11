imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { AppRemote } = imports.clapper_src.appRemote;
const Misc = imports.clapper_src.misc;

const opts = {
    idPostfix: 'Remote',
};

Misc.clapperPath = pkg.datadir + '/' +
    pkg.name.substring(0, pkg.name.lastIndexOf(opts.idPostfix));

function main()
{
    new AppRemote(opts).run();
}
