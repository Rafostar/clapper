const { Gio, GLib } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

Gio._promisify(Gio._LocalFilePrototype, 'make_directory_async', 'make_directory_finish');

function createCacheDirPromise()
{
    return new Promise(async (resolve, reject) => {
        const cacheDir = Gio.File.new_for_path(
            GLib.get_user_cache_dir() + '/' + Misc.appId
        );

        if(cacheDir.query_exists(null))
            return resolve(cacheDir);

        const dirCreated = await cacheDir.make_directory_async(
            GLib.PRIORITY_DEFAULT,
            null,
        ).catch(debug);

        if(!dirCreated)
            return reject(new Error(`could not create dir: ${cacheDir.get_path()}`));

        resolve(cacheDir);
    });
}
