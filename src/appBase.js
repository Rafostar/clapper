const { Gio, GLib, GObject, Gtk } = imports.gi;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;
const Actions = imports.src.actions;

const { debug } = Debug;
const { settings } = Misc;

var AppBase = GObject.registerClass({
    GTypeName: 'ClapperAppBase',
},
class ClapperAppBase extends Gtk.Application
{
    _init()
    {
        super._init({
            application_id: Misc.appId,
        });

        this.doneFirstActivate = false;
        this.isFileAppend = false;
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        const window = new Gtk.ApplicationWindow({
            application: this,
            title: Misc.appName,
        });

        /* FIXME: AFAIK there is no way to detect theme rounded corners.
         * Having 2/4 corners rounded in floating mode is not good. */
        window.add_css_class('adwrounded');

        if(!settings.get_boolean('render-shadows'))
            window.add_css_class('gpufriendly');

        window.add_css_class('gpufriendlyfs');
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        if(!this.doneFirstActivate)
            this._onFirstActivate();

        this.active_window.present_with_time(
            Math.floor(GLib.get_monotonic_time() / 1000)
        );
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
        for(let name in Actions.actions) {
            const simpleAction = new Gio.SimpleAction({ name });
            simpleAction.connect('activate', (action) =>
                Actions.handleAction(action, this.active_window)
            );
            this.add_action(simpleAction);

            const accels = Actions.actions[name];
            if(accels)
                this.set_accels_for_action(`app.${name}`, accels);
        }
        this.doneFirstActivate = true;
    }
});
