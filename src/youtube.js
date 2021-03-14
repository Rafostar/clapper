const { GLib, GObject, Gst, Soup } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;
const YTDL = imports.src.assets['node-ytdl-core'];

const { debug } = Debug;

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
        });

        /* videoID of current active download */
        this.downloadingVideoId = null;

        this.lastInfo = null;
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

            let tries = 2;
            while(tries--) {
                debug(`obtaining YouTube video info: ${videoId}`);
                this.downloadingVideoId = videoId;

                const [info, isAborted] = await this._getInfoPromise(videoId).catch(debug);
                if(!info) {
                    if(isAborted)
                        return reject(new Error('download aborted'));

                    debug(`failed, remaining tries: ${tries}`);
                    continue;
                }

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

                const isCipher = this._getIsCipher(info.streamingData);
                if(isCipher) {
                    debug('video requires deciphering');

                    const embedUri = `https://www.youtube.com/embed/${videoId}`;
                    const [body, isAbortedBody] =
                        await this._downloadDataPromise(embedUri).catch(debug);

                    if(isAbortedBody)
                        break;

                    /* We need matching info, so start from beginning */
                    if(!body)
                        continue;

                    const ytPath = body.match(/(?<=jsUrl\":\").*?(?=\")/gs)[0];
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

                    /* TODO: cache */
                    let actions;

                    if(!actions) {
                        const [pBody, isAbortedPlayer] =
                            await this._downloadDataPromise(ytUri).catch(debug);
                        if(!pBody || isAbortedPlayer) {
                            debug(new Error('could not download player body'));
                            break;
                        }
                        actions = YTDL.sig.extractActions(pBody);
                    }

                    if(!actions || !actions.length) {
                        debug(new Error('could not extract decipher actions'));
                        break;
                    }
                    debug('successfully obtained decipher actions');
                    const isDeciphered = this._decipherStreamingData(
                        info.streamingData, actions
                    );
                    if(!isDeciphered) {
                        debug('streaming data could not be deciphered');
                        break;
                    }
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
            let data = '';

            const chunkSignal = message.connect('got-chunk', (msg, chunk) => {
                debug(`got chunk of data, length: ${chunk.length}`);

                const chunkData = chunk.get_data();
                data += (chunkData instanceof Uint8Array)
                    ? ByteArray.toString(chunkData)
                    : chunkData;
            });

            this.queue_message(message, (session, msg) => {
                msg.disconnect(chunkSignal);

                debug('got message response');
                const statusCode = msg.status_code;

                /* Internal Soup codes mean download aborted
                 * or some other error that cannot be handled
                 * and we do not want to retry in such case */
                if(statusCode < 10)
                    return resolve([null, true]);

                if(statusCode !== 200)
                    return reject(new Error(`response code: ${statusCode}`));

                resolve([data, false]);
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

            this._downloadDataPromise(url).then(res => {
                if(res[1])
                    return resolve([null, true]);

                debug('parsing video info JSON');

                const gstUri = Gst.Uri.from_string('?' + res[0]);

                if(!gstUri)
                    return reject(new Error('could not convert query to URI'));

                const playerResponse = gstUri.get_query_value('player_response');

                if(!playerResponse)
                    return reject(new Error('no player response in query'));

                let info = null;

                try { info = JSON.parse(playerResponse); }
                catch(err) { debug(err.message) }

                if(!info)
                    return reject(new Error('could not parse video info JSON'));

                debug('successfully parsed video info JSON');
                resolve([info, false]);
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

        debug('stream deciphered');

        return `${url}&${sig}=${key}`;
    }
});

function checkYouTubeUri(uri)
{
    const gstUri = Gst.Uri.from_string(uri);
    gstUri.normalize();

    const host = gstUri.get_host();

    let success = true;
    let videoId = null;

    switch(host) {
        case 'www.youtube.com':
        case 'youtube.com':
            videoId = gstUri.get_query_value('v');
            break;
        case 'youtu.be':
            videoId = gstUri.get_path_segments()[1];
            break;
        default:
            success = false;
            break;
    }

    return [success, videoId];
}
