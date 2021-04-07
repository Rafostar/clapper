const { Gio, GObject, Gtk } = imports.gi;
const { AppBase } = imports.src.appBase;
const { Widget } = imports.src.widget;
const Debug = imports.src.debug;

const { debug } = Debug;

var App = GObject.registerClass(
class ClapperApp extends AppBase
{
    _init()
    {
        super._init();

        this.flags |= Gio.ApplicationFlags.HANDLES_OPEN;
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        const window = this.active_window;
        const clapperWidget = new Widget();
        const dummyHeaderbar = new Gtk.Box({
            can_focus: false,
            focusable: false,
            visible: false,
        });

        window.add_css_class('nobackground');
        window.set_child(clapperWidget);
        window.set_titlebar(dummyHeaderbar);
    }

    vfunc_open(files, hint)
    {
        super.vfunc_open(files, hint);

        this._openFiles(files);
        this.activate();
    }
});
