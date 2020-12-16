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

        super._init({ flags });

        this.remoteApp = null;
        this.isRemoteClosing = false;

        this.setenv('GDK_BACKEND', 'broadway', true);
    }

    startRemoteApp()
    {
        if(this.remoteApp)
            return;

        this.remoteApp = this.spawnv([Misc.appId + '.Remote']);
        this.remoteApp.wait_async(null, this._onRemoteClosed.bind(this));

        debug('remote app started');
    }

    stopRemoteApp()
    {
        if(!this.remoteApp || this.isRemoteClosing)
            return;

        this.isRemoteClosing = true;
        this.remoteApp.force_exit();

        debug('send stop signal to remote app');
    }

    _onRemoteClosed(proc, result)
    {
        let hadError;

        try {
            hadError = proc.wait_finish(result);
        }
        catch(err) {
            debug(err);
        }

        this.remoteApp = null;
        this.isRemoteClosing = false;

        if(hadError)
            debug('remote app exited with error');

        debug('remote app closed');
    }
});
