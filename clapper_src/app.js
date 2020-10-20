const { Gio, GObject, Gtk } = imports.gi;
const { HeaderBar } = imports.clapper_src.headerbar;
const { Widget } = imports.clapper_src.widget;
const Menu = imports.clapper_src.menu;

const APP_NAME = 'Clapper' || pkg.name.substring(
    pkg.name.lastIndexOf('.') + 1
);

var App = GObject.registerClass(
class ClapperApp extends Gtk.Application
{
    _init(opts)
    {
        super._init({
            application_id: pkg.name
        });

        let defaults = {
            playlist: [],
        };
        Object.assign(this, defaults, opts);
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        let window = new Gtk.ApplicationWindow({
            application: this,
            title: APP_NAME,
        });

        for(let action of Menu.actions) {
            let simpleAction = new Gio.SimpleAction({
                name: action.name
            });
            simpleAction.connect('activate', () =>
                action(this.active_window, APP_NAME)
            );
            this.add_action(simpleAction);
        }
        let uiBuilder = Gtk.Builder.new_from_file(
            `${pkg.datadir}/${pkg.name}/ui/clapper.ui`
        );
        let models = {
            settingsMenu: uiBuilder.get_object('settingsMenu')
        };
        let headerBar = new HeaderBar(window, models);
        window.set_titlebar(headerBar);

        let clapperWidget = new Widget();
        window.set_child(clapperWidget);
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        this.windowShowSignal = this.active_window.connect(
            'show', this._onWindowShow.bind(this)
        );
        this.active_window.present();
    }

    run(arr)
    {
        super.run(arr || []);
    }

    _onWindowShow(window)
    {
         window.disconnect(this.windowShowSignal);
         this.windowShowSignal = null;

         if(this.playlist.length) {
            let { player } = window.get_child();
            player.set_playlist(this.playlist);
         }
    }
});
