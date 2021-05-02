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
Gio._promisify(LocalFilePrototype, 'query_info_async', 'query_info_finish');
Gio._promisify(LocalFilePrototype, 'enumerate_children_async', 'enumerate_children_finish');

Gio._promisify(Gio.FileEnumerator.prototype, 'close_async', 'close_finish');
Gio._promisify(Gio.FileEnumerator.prototype, 'next_files_async', 'next_files_finish');

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

/* Simple save data to GioFile */
function saveFileSimplePromise(file, data)
{
    return file.replace_contents_bytes_async(
        GLib.Bytes.new_take(data),
        null,
        false,
        Gio.FileCreateFlags.NONE,
        null
    );
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
        saveFileSimplePromise(destFile, data)
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

function _getDirUrisPromise(dir, isDeep)
{
    return new Promise(async (resolve, reject) => {
        const enumerator = await dir.enumerate_children_async(
            'standard::name,standard::type',
            Gio.FileQueryInfoFlags.NONE,
            GLib.PRIORITY_DEFAULT,
            null
        ).catch(debug);

        if(!enumerator)
            return reject(new Error('could not create file enumerator'));

        const dirPath = dir.get_path();
        const arr = [];

        debug(`enumerating files in dir: ${dirPath}`);

        while(true) {
            const infos = await enumerator.next_files_async(
                1,
                GLib.PRIORITY_DEFAULT,
                null
            ).catch(debug);

            if(!infos || !infos.length)
                break;

            const fileUri = dir.get_uri() + '/' + infos[0].get_name();

            if(infos[0].get_file_type() !== Gio.FileType.DIRECTORY) {
                arr.push(fileUri);
                continue;
            }
            if(!isDeep)
                continue;

            const subDir = Misc.getFileFromLocalUri(fileUri);
            const subDirUris = await _getDirUrisPromise(subDir, isDeep).catch(debug);

            if(subDirUris && subDirUris.length)
                arr.push(...subDirUris);
        }

        const isClosed = await enumerator.close_async(
            GLib.PRIORITY_DEFAULT,
            null
        ).catch(debug);

        if(isClosed)
            debug(`closed enumerator for dir: ${dirPath}`);
        else
            debug(new Error(`could not close file enumerator for dir: ${dirPath}`));

        resolve(arr);
    });
}

/* Either GioFile or URI for dir arg */
function getDirFilesUrisPromise(dir, isDeep)
{
    return new Promise(async (resolve, reject) => {
        if(!dir.get_path)
            dir = Misc.getFileFromLocalUri(dir);
        if(!dir)
            return reject(new Error('invalid directory'));

        const fileInfo = await dir.query_info_async(
            'standard::type',
            Gio.FileQueryInfoFlags.NONE,
            GLib.PRIORITY_DEFAULT,
            null
        ).catch(debug);

        if(!fileInfo)
            return reject(new Error('no file type info'));

        if(fileInfo.get_file_type() !== Gio.FileType.DIRECTORY)
            return resolve([dir.get_uri()]);

        const arr = await _getDirUrisPromise(dir, isDeep).catch(debug);
        if(!arr || !arr.length)
            return reject(new Error('enumerated files list is empty'));

        resolve(arr.sort());
    });
}
