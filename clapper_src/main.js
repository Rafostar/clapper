imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { Gst } = imports.gi;
const { App } = imports.clapper_src.app;

Gst.init(null);

function main(argv)
{
    new App().run(argv);
}
