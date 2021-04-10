imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';
imports.gi.versions.Soup = '2.4';

const { Gst } = imports.gi;
Gst.init(null);

const { App } = imports.src.app;

function main(argv)
{
    new App().run(argv);
}
