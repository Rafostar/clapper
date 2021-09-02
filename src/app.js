const { Gio, GObject, Gtk } = imports.gi;
const { AppBase } = imports.src.appBase;
const { Widget } = imports.src.widget;
const Debug = imports.src.debug;

const { debug } = Debug;

var App = GObject.registerClass({
    GTypeName: 'ClapperApp',
},
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

        this.mapSignal = window.connect('map', this._onWindowMap.bind(this));
    }

    vfunc_open(files, hint)
    {
        super.vfunc_open(files, hint);

        this._openFilesAsync(files).then(() => this.activate()).catch(debug);
    }

    _onWindowMap(window)
    {
        window.disconnect(this.mapSignal);
        this.mapSignal = null;

        window.child._onWindowMap(window);
    }
});
