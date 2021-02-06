const { Gio, GObject } = imports.gi;
const { AppBase } = imports.src.appBase;
const { HeaderBar } = imports.src.headerbar;
const { Widget } = imports.src.widget;
const Debug = imports.src.debug;

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
    }

    vfunc_open(files, hint)
    {
        super.vfunc_open(files, hint);

        const { player } = this.active_window.get_child();

        if(!this.doneFirstActivate)
            player._preparePlaylist(files);
        else
            player.set_playlist(files);

        this.activate();
    }

    _onWindowShow(window)
    {
        super._onWindowShow(window);

        const { player } = this.active_window.get_child();
        const success = player.playlistWidget.nextTrack();

        if(!success)
            debug('playlist is empty');

        player.widget.grab_focus();
    }
});
