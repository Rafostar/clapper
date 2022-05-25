const { Gio, GObject, Gtk } = imports.gi;
const { Widget } = imports.src.widget;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;
const Actions = imports.src.actions;

const { debug } = Debug;
const { settings } = Misc;

var App = GObject.registerClass({
    GTypeName: 'ClapperApp',
},
class ClapperApp extends Gtk.Application
{
    _init()
    {
        super._init({
            application_id: Misc.appId,
            flags: Gio.ApplicationFlags.HANDLES_OPEN,
        });

        this.doneFirstActivate = false;
        this.isFileAppend = false;
        this.mapSignal = null;
    }

    vfunc_open(files, hint)
    {
        super.vfunc_open(files, hint);

        this.activate();
        this._openFilesAsync(files).catch(debug);
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        if(!this.doneFirstActivate)
            this._onFirstActivate();

        this.active_window.present();
    }

    async _openFilesAsync(files)
    {
        const urisArr = [];

        for(let file of files) {
            const uri = file.get_uri();
            if(!uri.startsWith('file:')) {
                urisArr.push(uri);
                continue;
            }

            /* If file is not a dir its URI will be returned in an array */
            const uris = await FileOps.getDirFilesUrisPromise(file).catch(debug);
            if(uris && uris.length)
                urisArr.push(...uris);
        }

        const [playlist, subs] = Misc.parsePlaylistFiles(urisArr);
        const { player } = this.active_window.get_child();
        const action = (this.isFileAppend) ? 'append' : 'set';

        if(playlist && playlist.length)
            player[`${action}_playlist`](playlist);
        if(subs)
            player.set_subtitles(subs);

        /* Restore default behavior */
        this.isFileAppend = false;
    }

    _onFirstActivate()
    {
        const window = new Gtk.ApplicationWindow({
            application: this,
            title: Misc.appName,
        });

        window.add_css_class('adwrounded');

        if(!settings.get_boolean('render-shadows'))
            window.add_css_class('gpufriendly');

        window.add_css_class('gpufriendlyfs');

        const clapperWidget = new Widget();
        const dummyHeaderbar = new Gtk.Box({
            can_focus: false,
            focusable: false,
            visible: false,
        });

        window.add_css_class('nobackground');
        window.set_child(clapperWidget);
        window.set_titlebar(dummyHeaderbar);

        for(let name in Actions.actions) {
            const simpleAction = new Gio.SimpleAction({ name });
            simpleAction.connect('activate', (action) =>
                Actions.handleAction(action, window)
            );
            this.add_action(simpleAction);

            const accels = Actions.actions[name];
            if(accels)
                this.set_accels_for_action(`app.${name}`, accels);
        }

        this.mapSignal = window.connect('map', this._onWindowMap.bind(this));
        this.doneFirstActivate = true;
    }

    _onWindowMap(window)
    {
        window.disconnect(this.mapSignal);
        this.mapSignal = null;

        debug('window mapped');

        window.child._onWindowMap(window);
    }
});
