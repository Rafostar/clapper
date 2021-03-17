const { Gio, GLib } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

Gio._promisify(Gio._LocalFilePrototype, 'make_directory_async', 'make_directory_finish');

function createCacheDirPromise()
{
    const dir = Gio.File.new_for_path(
        GLib.get_user_cache_dir() + '/' + Misc.appId
    );

    return createDirPromise(dir);
}

function createTempDirPromise()
{
    const dir = Gio.File.new_for_path(
        GLib.get_tmp_dir() + '/.' + Misc.appId
    );

    return createDirPromise(dir);
}

function createDirPromise(dir)
{
    return new Promise(async (resolve, reject) => {
        if(dir.query_exists(null))
            return resolve(dir);

        const dirCreated = await dir.make_directory_async(
            GLib.PRIORITY_DEFAULT,
            null,
        ).catch(debug);

        if(!dirCreated)
            return reject(new Error(`could not create dir: ${dir.get_path()}`));

        resolve(dir);
    });
}
