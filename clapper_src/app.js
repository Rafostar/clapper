const { Gio, GObject, Gtk } = imports.gi;
const { HeaderBar } = imports.clapper_src.headerbar;
const { Widget } = imports.clapper_src.widget;
const Debug = imports.clapper_src.debug;
const Menu = imports.clapper_src.menu;
const Misc = imports.clapper_src.misc;

let { debug } = Debug;
let { settings } = Misc;

var App = GObject.registerClass(
class ClapperApp extends Gtk.Application
{
    _init(opts)
    {
        super._init({
            application_id: Misc.appId
        });

        let defaults = {
            playlist: [],
        };
        Object.assign(this, defaults, opts);

        this.doneFirstActivate = false;
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        let window = new Gtk.ApplicationWindow({
            application: this,
            title: Misc.appName,
        });
        window.isClapperApp = true;
        window.add_css_class('nobackground');

        if(!settings.get_boolean('render-shadows'))
            window.add_css_class('gpufriendly');

        for(let action in Menu.actions) {
            let simpleAction = new Gio.SimpleAction({
                name: action
            });
            simpleAction.connect(
                'activate', () => Menu.actions[action](this.active_window)
            );
            this.add_action(simpleAction);
        }

        let clapperWidget = new Widget();
        window.set_child(clapperWidget);

        let size = clapperWidget.windowSize;
        window.set_default_size(size[0], size[1]);
        debug(`restored window size: ${size[0]}x${size[1]}`);

        let clapperPath = Misc.getClapperPath();
        let uiBuilder = Gtk.Builder.new_from_file(
            `${clapperPath}/ui/clapper.ui`
        );
        let models = {
            addMediaMenu: uiBuilder.get_object('addMediaMenu'),
            settingsMenu: uiBuilder.get_object('settingsMenu'),
        };
        let headerBar = new HeaderBar(window, models);
        window.set_titlebar(headerBar);
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        if(!this.doneFirstActivate)
            this._onFirstActivate();

        this.active_window.present();
    }

    run(arr)
    {
        super.run(arr || []);
    }

    _onFirstActivate()
    {
        let gtkSettings = Gtk.Settings.get_default();
        settings.bind(
            'dark-theme', gtkSettings,
            'gtk_application_prefer_dark_theme',
            Gio.SettingsBindFlags.GET
        );

        this.windowShowSignal = this.active_window.connect(
            'show', this._onWindowShow.bind(this)
        );

        this.doneFirstActivate = true;
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
