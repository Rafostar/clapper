imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const System = imports.system;
const { Gst } = imports.gi;
const { App } = imports.clapper_src.app;

Gst.init(null);

function main()
{
    new App().run([System.programInvocationName].concat(ARGV));
}
