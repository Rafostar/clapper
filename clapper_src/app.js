const { GObject } = imports.gi;
const { AppBase } = imports.clapper_src.appBase;
const { HeaderBar } = imports.clapper_src.headerbar;
const { Widget } = imports.clapper_src.widget;
const Debug = imports.clapper_src.debug;

let { debug } = Debug;

var App = GObject.registerClass(
class ClapperApp extends AppBase
{
    vfunc_startup()
    {
        super.vfunc_startup();

        this.active_window.isClapperApp = true;
        this.active_window.add_css_class('nobackground');

        let clapperWidget = new Widget();
        this.active_window.set_child(clapperWidget);

        let headerBar = new HeaderBar(this.active_window);
        this.active_window.set_titlebar(headerBar);

        let size = clapperWidget.windowSize;
        this.active_window.set_default_size(size[0], size[1]);
        debug(`restored window size: ${size[0]}x${size[1]}`);
    }

    _onWindowShow(window)
    {
        super._onWindowShow(window);

        if(this.playlist.length) {
            let { player } = window.get_child();
            player.set_playlist(this.playlist);
        }
    }
});
