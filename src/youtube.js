const { Gio, GLib, GObject, Gst, Soup } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;
const YTDL = imports.src.assets['node-ytdl-core'];

const { debug } = Debug;

const InitAsyncState = {
    NONE: 0,
    IN_PROGRESS: 1,
    DONE: 2,
};

var YouTubeClient = GObject.registerClass({
    Signals: {
        'info-resolved': {
            param_types: [GObject.TYPE_BOOLEAN]
        }
    }
}, class ClapperYouTubeClient extends Soup.Session
{
    _init()
    {
        super._init({
            timeout: 5,
            max_conns_per_host: 1,
            /* TODO: share this with GstClapper lib (define only once) */
            user_agent: 'Mozilla/5.0 (X11; Linux x86_64; rv:86.0) Gecko/20100101 Firefox/86.0',
        });
        this.initAsyncState = InitAsyncState.NONE;

        /* videoID of current active download */
        this.downloadingVideoId = null;

        this.lastInfo = null;
        this.cachedSig = {
            id: null,
            actions: null,
        };
    }

    getVideoInfoPromise(videoId)
    {
        /* If in middle of download and same videoID,
         * resolve to current download */
        if(
            this.downloadingVideoId
            && this.downloadingVideoId === videoId
        )
            return this._getCurrentDownloadPromise();

        return new Promise(async (resolve, reject) => {
            /* Do not redownload info for the same video */
            if(this.compareLastVideoId(videoId))
                return resolve(this.lastInfo);

            this.abort();

            /* Prevent doing this code more than once at a time */
            if(this.initAsyncState === InitAsyncState.NONE) {
                this.initAsyncState = InitAsyncState.IN_PROGRESS;

                debug('loading cookies DB');
                const cacheDir = await FileOps.createCacheDirPromise().catch(debug);
                if(!cacheDir) {
                    this.initAsyncState = InitAsyncState.NONE;
                    return reject(new Error('could not create cookies DB'));
                }

                const cookiesDB = new Soup.CookieJarDB({
                    filename: cacheDir.get_child('cookies.sqlite').get_path(),
                    read_only: false,
                });
                this.add_feature(cookiesDB);
                debug('successfully loaded cookies DB');

                this.initAsyncState = InitAsyncState.DONE;
            }

            /* Too many tries might trigger 429 ban,
             * leave while with break as a "goto" replacement */
            let tries = 1;
            while(tries--) {
                debug(`obtaining YouTube video info: ${videoId}`);
                this.downloadingVideoId = videoId;

                let result;
                let isFoundInTemp = false;

                const tempInfo = await this._getFileContentsPromise('tmp', 'yt-info', videoId).catch(debug);
                if(tempInfo) {
                    debug('checking temp info for requested video');
                    let parsedTempInfo;

                    try { parsedTempInfo = JSON.parse(tempInfo); }
                    catch(err) { debug(err); }

                    if(parsedTempInfo) {
                        const nowSeconds = Math.floor(Date.now() / 1000);
                        const { expireDate } = parsedTempInfo.streamingData;

                        if(expireDate && expireDate > nowSeconds) {
                            debug(`found usable info, remaining live: ${expireDate - nowSeconds}`);

                            isFoundInTemp = true;
                            result = { data: parsedTempInfo };
                        }
                        else
                            debug('temp info expired');
                    }
                }

                if(!result)
                    result = await this._getInfoPromise(videoId).catch(debug);

                if(!result || !result.data) {
                    if(result && result.isAborted)
                        debug(new Error('download aborted'));

                    break;
                }
                const info = result.data;

                const invalidInfoMsg = (
                    !info.playabilityStatus
                    || !info.playabilityStatus.status === 'OK'
                )
                    ? 'video is not playable'
                    : (!info.streamingData)
                    ? 'video response data is missing streaming data'
                    : null;

                if(invalidInfoMsg) {
                    this.lastInfo = null;

                    debug(new Error(invalidInfoMsg));
                    break;
                }

                /* Make sure we have all formats arrays,
                 * so we will not have to keep checking */
                if(!info.streamingData.formats)
                    info.streamingData.formats = [];
                if(!info.streamingData.adaptiveFormats)
                    info.streamingData.adaptiveFormats = [];

                if(this._getIsCipher(info.streamingData)) {
                    debug('video requires deciphering');

                    /* Decipher actions do not change too often, so try
                     * to reuse without triggering too many requests ban */
                    let actions = this.cachedSig.actions;

                    if(actions)
                        debug('using remembered decipher actions');
                    else {
                        const embedUri = `https://www.youtube.com/embed/${videoId}`;
                        result = await this._downloadDataPromise(embedUri).catch(debug);

                        if(result && result.isAborted)
                            break;
                        else if(!result || !result.data) {
                            debug(new Error('could not download embed body'));
                            break;
                        }

                        const ytPath = result.data.match(/(?<=jsUrl\":\").*?(?=\")/gs)[0];
                        if(!ytPath) {
                            debug(new Error('could not find YouTube player URI'));
                            break;
                        }
                        const ytUri = `https://www.youtube.com${ytPath}`;
                        if(
                            /* check if site has "/" after ".com" */
                            ytUri[23] !== '/'
                            || !Gst.Uri.is_valid(ytUri)
                        ) {
                            debug(`misformed player URI: ${ytUri}`);
                            break;
                        }
                        debug(`found player URI: ${ytUri}`);

                        const ytId = ytPath.split('/').find(el => Misc.isHex(el));
                        actions = await this._getFileContentsPromise('user_cache', 'yt-sig', ytId).catch(debug);

                        if(!actions) {
                            result = await this._downloadDataPromise(ytUri).catch(debug);

                            if(result && result.isAborted)
                                break;
                            else if(!result || !result.data) {
                                debug(new Error('could not download player body'));
                                break;
                            }

                            actions = YTDL.sig.extractActions(result.data);
                            if(actions) {
                                debug('deciphered, saving cipher actions to cache file');
                                this._createSubdirFileAsync('user_cache', 'yt-sig', ytId, actions);
                            }
                        }
                        if(!actions || !actions.length) {
                            debug(new Error('could not extract decipher actions'));
                            break;
                        }
                        if(this.cachedSig.id !== ytId) {
                            this.cachedSig.id = ytId;
                            this.cachedSig.actions = actions;
                        }
                    }
                    debug(`successfully obtained decipher actions: ${actions}`);

                    const isDeciphered = this._decipherStreamingData(
                        info.streamingData, actions
                    );
                    if(!isDeciphered) {
                        debug('streaming data could not be deciphered');
                        break;
                    }
                }

                if(!isFoundInTemp) {
                    const exp = info.streamingData.expiresInSeconds || 0;
                    const len = info.videoDetails.lengthSeconds || 3;

                    /* Estimated safe time for rewatching video */
                    info.streamingData.expireDate = Math.floor(Date.now() / 1000)
                        + Number(exp) - (3 * len);

                    this._createSubdirFileAsync(
                        'tmp', 'yt-info', videoId, JSON.stringify(info)
                    );
                }

                this.lastInfo = info;
                this.emit('info-resolved', true);
                this.downloadingVideoId = null;

                return resolve(info);
            }

            /* Do not clear video info here, as we might still have
             * valid info from last video that can be reused */
            this.emit('info-resolved', false);
            this.downloadingVideoId = null;

            reject(new Error('could not obtain YouTube video info'));
        });
    }

    getBestCombinedUri(info)
    {
        if(!info.streamingData.formats.length)
            return null;

        const combinedStream = info.streamingData.formats[
            info.streamingData.formats.length - 1
        ];

        if(!combinedStream || !combinedStream.url)
            return null;

        return combinedStream.url;
    }

    compareLastVideoId(videoId)
    {
        if(!this.lastInfo)
            return false;

        if(
            !this.lastInfo
            || !this.lastInfo.videoDetails
            || this.lastInfo.videoDetails.videoId !== videoId
            /* TODO: check if video expired */
        )
            return false;

        return true;
    }

    _downloadDataPromise(url)
    {
        return new Promise((resolve, reject) => {
            const message = Soup.Message.new('GET', url);
            const result = {
                data: '',
                isAborted: false,
            };

            const chunkSignal = message.connect('got-chunk', (msg, chunk) => {
                debug(`got chunk of data, length: ${chunk.length}`);

                const chunkData = chunk.get_data();
                if(!chunkData) return;

                result.data += (chunkData instanceof Uint8Array)
                    ? ByteArray.toString(chunkData)
                    : chunkData;
            });

            this.queue_message(message, (session, msg) => {
                msg.disconnect(chunkSignal);

                debug('got message response');
                const statusCode = msg.status_code;

                if(statusCode === 200)
                    return resolve(result);

                debug(new Error(`response code: ${statusCode}`));

                /* Internal Soup codes mean download aborted
                 * or some other error that cannot be handled
                 * and we do not want to retry in such case */
                if(statusCode < 10 || statusCode === 429) {
                    result.isAborted = true;
                    return resolve(result);
                }

                return reject(new Error('could not download data'));
            });
        });
    }

    _getCurrentDownloadPromise()
    {
        debug('resolving after current download finishes');

        return new Promise((resolve, reject) => {
            const infoResolvedSignal = this.connect('info-resolved', (self, success) => {
                this.disconnect(infoResolvedSignal);

                debug('current download finished, resolving');

                if(!success)
                    return reject(new Error('info resolve was unsuccessful'));

                /* At this point new video info is set */
                resolve(this.lastInfo);
            });
        });
    }

    _getInfoPromise(videoId)
    {
        return new Promise((resolve, reject) => {
            const query = [
                `video_id=${videoId}`,
                `el=embedded`,
                `eurl=https://youtube.googleapis.com/v/${videoId}`,
            ].join('&');
            const url = `https://www.youtube.com/get_video_info?${query}`;

            this._downloadDataPromise(url).then(result => {
                if(result.isAborted)
                    return resolve(result);

                debug('parsing video info JSON');

                const gstUri = Gst.Uri.from_string('?' + result.data);

                if(!gstUri)
                    return reject(new Error('could not convert query to URI'));

                const playerResponse = gstUri.get_query_value('player_response');

                if(!playerResponse)
                    return reject(new Error('no player response in query'));

                let info = null;

                try { info = JSON.parse(playerResponse); }
                catch(err) { debug(err.message); }

                if(!info)
                    return reject(new Error('could not parse video info JSON'));

                debug('successfully parsed video info JSON');
                result.data = info;

                resolve(result);
            })
            .catch(err => reject(err));
        });
    }

    _getIsCipher(data)
    {
        /* Check only first best combined,
         * AFAIK there are no videos without it */
        if(data.formats[0].url)
            return false;

        if(
            data.formats[0].signatureCipher
            || data.formats[0].cipher
        )
            return true;

        /* FIXME: no URLs and no cipher, what now? */
        debug(new Error('no url or cipher in streams'));

        return false;
    }

    _decipherStreamingData(data, actions)
    {
        debug('checking cipher query keys');

        /* Cipher query keys should be the same for all
         * streams, so parse any stream to get their names */
        const anyStream = data.formats[0] || data.adaptiveFormats[0];
        const sigQuery = anyStream.signatureCipher || anyStream.cipher;

        if(!sigQuery)
            return false;

        const gstUri = Gst.Uri.from_string('?' + sigQuery);
        const queryKeys = gstUri.get_query_keys();

        const cipherKey = queryKeys.find(key => {
            const value = gstUri.get_query_value(key);
            /* A long value that is not URI */
            return (
                value.length > 32
                && !Gst.Uri.is_valid(value)
            );
        });
        if(!cipherKey) {
            debug('no stream cipher key name');
            return false;
        }

        const sigKey = queryKeys.find(key => {
            const value = gstUri.get_query_value(key);
            /* A short value that is not URI */
            return (
                value.length < 32
                && !Gst.Uri.is_valid(value)
            );
        });
        if(!sigKey) {
            debug('no stream signature key name');
            return false;
        }

        const urlKey = queryKeys.find(key =>
            Gst.Uri.is_valid(gstUri.get_query_value(key))
        );
        if(!urlKey) {
            debug('no stream URL key name');
            return false;
        }

        const cipherKeys = {
            url: urlKey,
            sig: sigKey,
            cipher: cipherKey,
        };

        debug('deciphering streams');

        for(let format of [data.formats, data.adaptiveFormats]) {
            for(let stream of format) {
                const formatUrl = this._getDecipheredUrl(
                    stream, actions, cipherKeys
                );
                if(!formatUrl) {
                    debug('undecipherable stream');
                    debug(stream);

                    return false;
                }
                stream.url = formatUrl;
            }
        }
        debug('all streams deciphered');

        return true;
    }

    _getDecipheredUrl(stream, actions, queryKeys)
    {
        debug(`deciphering stream id: ${stream.itag}`);

        const sigQuery = stream.signatureCipher || stream.cipher;
        if(!sigQuery) return null;

        const gstUri = Gst.Uri.from_string('?' + sigQuery);

        const url = gstUri.get_query_value(queryKeys.url);
        const cipher = gstUri.get_query_value(queryKeys.cipher);
        const sig = gstUri.get_query_value(queryKeys.sig);

        const key = YTDL.sig.decipher(cipher, actions);
        if(!key) return null;

        debug('stream deciphered');

        return `${url}&${sig}=${key}`;
    }

    async _createSubdirFileAsync(place, folderName, fileName, data)
    {
        const destDir = Gio.File.new_for_path([
            GLib[`get_${place}_dir`](),
            Misc.appId,
            folderName
        ].join('/'));

        for(let dir of [destDir.get_parent(), destDir]) {
            const createdDir = await FileOps.createDirPromise(dir).catch(debug);
            if(!createdDir) return;
        }

        const destFile = destDir.get_child(fileName);
        destFile.replace_contents_bytes_async(
            GLib.Bytes.new_take(data),
            null,
            false,
            Gio.FileCreateFlags.NONE,
            null
        )
        .then(() => debug(`saved file: ${destFile.get_path()}`))
        .catch(debug);
    }

    _getFileContentsPromise(place, folderName, fileName)
    {
        return new Promise((resolve, reject) => {
            debug(`reading data from ${place} file`);

            const file = Gio.File.new_for_path([
                GLib[`get_${place}_dir`](),
                Misc.appId,
                folderName,
                fileName
            ].join('/'));

            if(!file.query_exists(null)) {
                debug(`no such file: ${file.get_path()}`);
                return resolve(null);
            }

            file.load_bytes_async(null)
                .then(result => {
                    const data = result[0].get_data();
                    if(!data || !data.length)
                        return reject(new Error('source file is empty'));

                    if(data instanceof Uint8Array)
                        resolve(ByteArray.toString(data));
                    else
                        resolve(data);
                })
                .catch(err => reject(err));
        });
    }
});

function checkYouTubeUri(uri)
{
    const gstUri = Gst.Uri.from_string(uri);
    const originalHost = gstUri.get_host();
    gstUri.normalize();

    const host = gstUri.get_host();
    let videoId = null;

    switch(host) {
        case 'www.youtube.com':
        case 'youtube.com':
            videoId = gstUri.get_query_value('v');
            if(!videoId) {
                /* Handle embedded videos */
                const segments = gstUri.get_path_segments();
                if(segments && segments.length)
                    videoId = segments[segments.length - 1];
            }
            break;
        case 'youtu.be':
            videoId = gstUri.get_path_segments()[1];
            break;
        default:
            const scheme = gstUri.get_scheme();
            if(scheme === 'yt' || scheme === 'youtube') {
                /* ID is case sensitive */
                videoId = originalHost;
                break;
            }
            break;
    }

    const success = (videoId != null);

    return [success, videoId];
}
