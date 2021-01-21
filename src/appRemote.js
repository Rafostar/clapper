const { GObject } = imports.gi;
const { AppBase } = imports.src.appBase;
const { HeaderBarBase } = imports.src.headerbarBase;
const { WidgetRemote } = imports.src.widgetRemote;

var AppRemote = GObject.registerClass(
class ClapperAppRemote extends AppBase
{
    vfunc_startup()
    {
        super.vfunc_startup();

        const clapperWidget = new WidgetRemote();
        this.active_window.set_child(clapperWidget);

        const headerBar = new HeaderBarBase(this.active_window);
        this.active_window.set_titlebar(headerBar);

        this.active_window.maximize();
    }
});
