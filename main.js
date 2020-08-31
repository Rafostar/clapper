imports.gi.versions.Gtk = '3.0';

const { Gst } = imports.gi;
const { App } = imports.clapper_src.app;

Gst.init(null);

function main()
{
    let clapper = new App(arguments).run();
}
