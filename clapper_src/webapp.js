const { Gio, GObject } = imports.gi;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;

let { debug } = Debug;

var WebApp = GObject.registerClass(
class ClapperWebApp extends Gio.SubprocessLauncher
{
    _init()
    {
        const flags = Gio.SubprocessFlags.STDOUT_SILENCE
            | Gio.SubprocessFlags.STDERR_SILENCE;

        super._init(flags);
    }

    startRemoteApp()
    {
        this.setenv('GDK_BACKEND', 'broadway', true);
        this.setenv('BROADWAY_DISPLAY', '6', true);

        this.remoteApp = this.spawnv(Misc.appId);
        this.remoteApp.wait_async(null, this._onRemoteClosed.bind(this));

        debug('remote app started');
    }

    _onRemoteClosed(remoteApp, res)
    {
        debug('remote app closed');

        this.setenv('GDK_BACKEND', '', true);
        this.setenv('BROADWAY_DISPLAY', '', true);
    }
});
