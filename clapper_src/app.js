const { Gio, GObject } = imports.gi;
const { AppBase } = imports.clapper_src.appBase;
const { HeaderBar } = imports.clapper_src.headerbar;
const { Widget } = imports.clapper_src.widget;
const Debug = imports.clapper_src.debug;

const { debug } = Debug;

var App = GObject.registerClass(
class ClapperApp extends AppBase
{
    _init()
    {
        super._init();

        this.set_flags(
            this.get_flags()
            | Gio.ApplicationFlags.HANDLES_OPEN
        );
        this.playlist = [];
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        this.active_window.isClapperApp = true;
        this.active_window.add_css_class('nobackground');

        const clapperWidget = new Widget();
        this.active_window.set_child(clapperWidget);

        const headerBar = new HeaderBar(this.active_window);
        this.active_window.set_titlebar(headerBar);

        const size = clapperWidget.windowSize;
        this.active_window.set_default_size(size[0], size[1]);
        debug(`restored window size: ${size[0]}x${size[1]}`);
    }

    vfunc_open(files, hint)
    {
        super.vfunc_open(files, hint);

        this.playlist = files;

        if(this.doneFirstActivate)
            this.setWindowPlaylist(this.active_window);

        this.activate();
    }

    _onWindowShow(window)
    {
        super._onWindowShow(window);

        this.setWindowPlaylist(window);
    }

    setWindowPlaylist(window)
    {
        if(!this.playlist.length)
            return;

        const { player } = window.get_child();
        player.set_playlist(this.playlist);
    }
});
