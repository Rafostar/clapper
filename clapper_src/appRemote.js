const { GObject } = imports.gi;
const { AppBase } = imports.clapper_src.appBase;
const { HeaderBarBase } = imports.clapper_src.headerbarBase;
const { WidgetRemote } = imports.clapper_src.widgetRemote;

var AppRemote = GObject.registerClass(
class ClapperAppRemote extends AppBase
{
    vfunc_startup()
    {
        super.vfunc_startup();

        let clapperWidget = new WidgetRemote();
        this.active_window.set_child(clapperWidget);

        let headerBar = new HeaderBarBase(this.active_window);
        this.active_window.set_titlebar(headerBar);

        this.active_window.maximize();
    }
});
