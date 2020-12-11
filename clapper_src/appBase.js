const { Gio, GObject, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;
const Menu = imports.clapper_src.menu;
const Misc = imports.clapper_src.misc;

let { debug } = Debug;
let { settings } = Misc;

var AppBase = GObject.registerClass(
class ClapperAppBase extends Gtk.Application
{
    _init(opts)
    {
        opts = opts || {};

        let defaults = {
            idPostfix: '',
            playlist: [],
        };
        Object.assign(this, defaults, opts);

        super._init({
            application_id: Misc.appId + this.idPostfix
        });

        this.doneFirstActivate = false;
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        let window = new Gtk.ApplicationWindow({
            application: this,
            title: Misc.appName,
        });

        if(!settings.get_boolean('render-shadows'))
            window.add_css_class('gpufriendly');

        if(
            settings.get_boolean('dark-theme')
            && settings.get_boolean('brighter-sliders')
        )
            window.add_css_class('brightscale');

        for(let action in Menu.actions) {
            let simpleAction = new Gio.SimpleAction({
                name: action
            });
            simpleAction.connect(
                'activate', () => Menu.actions[action](this.active_window)
            );
            this.add_action(simpleAction);
        }
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
            'gtk-application-prefer-dark-theme',
            Gio.SettingsBindFlags.GET
        );
        this._onThemeChanged(gtkSettings);
        gtkSettings.connect('notify::gtk-theme-name', this._onThemeChanged.bind(this));

        this.windowShowSignal = this.active_window.connect(
            'show', this._onWindowShow.bind(this)
        );

        this.doneFirstActivate = true;
    }

    _onWindowShow(window)
    {
        window.disconnect(this.windowShowSignal);
        this.windowShowSignal = null;
    }

    _onThemeChanged(gtkSettings)
    {
        const theme = gtkSettings.gtk_theme_name;
        debug(`user selected theme: ${theme}`);

        if(!theme.endsWith('-dark'))
            return;

        /* We need to request a default theme with optional dark variant
           to make the "gtk_application_prefer_dark_theme" setting work */
        const parsedTheme = theme.substring(0, theme.lastIndexOf('-'));

        gtkSettings.gtk_theme_name = parsedTheme;
        debug(`set theme: ${parsedTheme}`);
    }
});
