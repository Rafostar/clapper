const { Gio, GLib } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

/* FIXME: Use Gio._LocalFilePrototype once we are safe to assume
 * that GJS with https://gitlab.gnome.org/GNOME/gjs/-/commit/ec9385b8 is used. */
const LocalFilePrototype = Gio.File.new_for_path('/').constructor.prototype;

Gio._promisify(LocalFilePrototype, 'load_bytes_async', 'load_bytes_finish');
Gio._promisify(LocalFilePrototype, 'make_directory_async', 'make_directory_finish');
Gio._promisify(LocalFilePrototype, 'replace_contents_bytes_async', 'replace_contents_finish');

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
        GLib.get_tmp_dir() + '/' + Misc.appId
    );

    return createDirPromise(dir);
}

/* Creates dir and resolves with it */
function createDirPromise(dir)
{
    return new Promise((resolve, reject) => {
        if(dir.query_exists(null))
            return resolve(dir);

        dir.make_directory_async(
            GLib.PRIORITY_DEFAULT,
            null
        )
        .then(success => {
            if(success)
                return resolve(dir);

            reject(new Error(`could not create dir: ${dir.get_path()}`));
        })
        .catch(err => reject(err));
    });
}

/* Saves file in optional subdirectory and resolves with it */
function saveFilePromise(place, subdirName, fileName, data)
{
    return new Promise(async (resolve, reject) => {
        let folderPath = GLib[`get_${place}_dir`]() + '/' + Misc.appId;

        if(subdirName)
            folderPath += `/${subdirName}`;

        const destDir = Gio.File.new_for_path(folderPath);
        const destPath = folderPath + '/' + fileName;

        debug(`saving file: ${destPath}`);

        const checkFolders = (subdirName)
            ? [destDir.get_parent(), destDir]
            : [destDir];

        for(let dir of checkFolders) {
            const createdDir = await createDirPromise(dir).catch(debug);
            if(!createdDir)
                return reject(new Error(`could not create dir: ${dir.get_path()}`));
        }

        const destFile = destDir.get_child(fileName);
        destFile.replace_contents_bytes_async(
            GLib.Bytes.new_take(data),
            null,
            false,
            Gio.FileCreateFlags.NONE,
            null
        )
        .then(() => {
            debug(`saved file: ${destPath}`);
            resolve(destFile);
        })
        .catch(err => reject(err));
    });
}

function getFileContentsPromise(place, subdirName, fileName)
{
    return new Promise((resolve, reject) => {
        let destPath = GLib[`get_${place}_dir`]() + '/' + Misc.appId;

        if(subdirName)
            destPath += `/${subdirName}`;

        destPath += `/${fileName}`;

        const file = Gio.File.new_for_path(destPath);
        debug(`reading data from: ${destPath}`);

        if(!file.query_exists(null)) {
            debug(`no such file: ${file.get_path()}`);
            return resolve(null);
        }

        file.load_bytes_async(null)
            .then(result => {
                const data = result[0].get_data();
                if(!data || !data.length)
                    return reject(new Error('source file is empty'));

                debug(`read data from: ${destPath}`);

                if(data instanceof Uint8Array)
                    resolve(ByteArray.toString(data));
                else
                    resolve(data);
            })
            .catch(err => reject(err));
    });
}
