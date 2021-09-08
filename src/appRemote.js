const { GObject } = imports.gi;
const { AppBase } = imports.appBase;
const { HeaderBarRemote } = imports.headerbarRemote;
const { WidgetRemote } = imports.widgetRemote;

var AppRemote = GObject.registerClass({
    GTypeName: 'ClapperAppRemote',
},
class ClapperAppRemote extends AppBase
{
    vfunc_startup()
    {
        super.vfunc_startup();

        const window = this.active_window;

        const clapperWidget = new WidgetRemote();
        window.set_child(clapperWidget);

        const headerBar = new HeaderBarRemote();
        window.set_titlebar(headerBar);

        window.maximize();
    }
});
