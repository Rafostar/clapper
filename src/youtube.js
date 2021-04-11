const { GObject, Gst, Soup } = imports.gi;
const Dash = imports.src.dash;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;
const YTDL = imports.src.assets['node-ytdl-core'];

const debug = Debug.ytDebug;

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
            timeout: 7,
            max_conns_per_host: 1,
            /* TODO: share this with GstClapper lib (define only once) */
            user_agent: 'Mozilla/5.0 (X11; Linux x86_64; rv:86.0) Gecko/20100101 Firefox/86.0',
        });
        this.initAsyncState = InitAsyncState.NONE;

        /* videoID of current active download */
        this.downloadingVideoId = null;

        this.lastInfo = null;
        this.postInfo = {
            clientVersion: null,
            visitorData: "",
        };

        this.cachedSig = {
            id: null,
            actions: null,
            timestamp: "",
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
                let isUsingPlayerResp = false;

                const tempInfo = await FileOps.getFileContentsPromise('tmp', 'yt-info', videoId).catch(debug);
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
                    result = await this._getPlayerInfoPromise(videoId).catch(debug);
                if(!result || !result.data) {
                    if(result && result.isAborted) {
                        debug(new Error('download aborted'));
                        break;
                    }
                }
                isUsingPlayerResp = (result != null);

                if(!result)
                    result = await this._getInfoPromise(videoId).catch(debug);
                if(!result || !result.data) {
                    if(result && result.isAborted)
                        debug(new Error('download aborted'));

                    break;
                }

                if(!isFoundInTemp) {
                    const [isPlayable, reason] = this._getPlayabilityStatus(result.data);

                    if(!isPlayable) {
                        debug(new Error(reason));
                        break;
                    }
                }

                let info = this._getReducedInfo(result.data);

                if(this._getIsCipher(info.streamingData)) {
                    debug('video requires deciphering');

                    /* Decipher actions do not change too often, so try
                     * to reuse without triggering too many requests ban */
                    let actions = this.cachedSig.actions;

                    if(actions)
                        debug('using remembered decipher actions');
                    else {
                        let sts = "";
                        const embedUri = `https://www.youtube.com/embed/${videoId}`;
                        result = await this._downloadDataPromise(embedUri).catch(debug);

                        if(result && result.isAborted)
                            break;
                        else if(!result || !result.data) {
                            debug(new Error('could not download embed body'));
                            break;
                        }

                        let ytPath = result.data.match(/jsUrl\":\"(.*?)\.js/g);
                        if(ytPath) {
                            ytPath = (ytPath[0] && ytPath[0].length > 16)
                                 ? ytPath[0].substring(8) : null;
                        }
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
                        let ytSigData = await FileOps.getFileContentsPromise(
                            'user_cache', 'yt-sig', ytId
                        ).catch(debug);

                        if(ytSigData) {
                            ytSigData = ytSigData.split(';');

                            if(ytSigData[0] && ytSigData[0] > 0) {
                                sts = ytSigData[0];
                                debug(`found local sts: ${sts}`);
                            }

                            const actionsIndex = (ytSigData.length > 1) ? 1 : 0;
                            actions = ytSigData[actionsIndex];
                        }

                        if(!actions) {
                            result = await this._downloadDataPromise(ytUri).catch(debug);

                            if(result && result.isAborted)
                                break;
                            else if(!result || !result.data) {
                                debug(new Error('could not download player body'));
                                break;
                            }

                            const stsArr = result.data.match(/signatureTimestamp[=\:]\d+/g);
                            if(stsArr) {
                                sts = (stsArr[0] && stsArr[0].length > 19)
                                    ? stsArr[0].substring(19) : null;

                                if(isNaN(sts) || sts <= 0)
                                    sts = "";
                                else
                                    debug(`extracted player sts: ${sts}`);
                            }

                            actions = YTDL.sig.extractActions(result.data);
                            if(actions) {
                                debug('deciphered, saving cipher actions to cache file');
                                const saveData = sts + ';' + actions;
                                /* We do not need to wait for it */
                                FileOps.saveFilePromise('user_cache', 'yt-sig', ytId, saveData);
                            }
                        }
                        if(!actions || !actions.length) {
                            debug(new Error('could not extract decipher actions'));
                            break;
                        }
                        if(this.cachedSig.id !== ytId) {
                            this.cachedSig.id = ytId;
                            this.cachedSig.actions = actions;
                            this.cachedSig.timestamp = sts;

                            /* Cipher info from player without timestamp is invalid
                             * so download it again now that we have a timestamp */
                            if(isUsingPlayerResp && sts > 0) {
                                debug(`redownloading player info with sts: ${sts}`);

                                result = await this._getPlayerInfoPromise(videoId).catch(debug);
                                if(!result || !result.data) {
                                    if(result && result.isAborted)
                                        debug(new Error('download aborted'));

                                    break;
                                }
                                info = this._getReducedInfo(result.data);
                            }
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
                    const dateSeconds = Math.floor(Date.now() / 1000);

                    /* Estimated safe time for rewatching video */
                    info.streamingData.expireDate = dateSeconds + Number(exp);

                    /* Last info is stored in variable, so don't wait here */
                    FileOps.saveFilePromise(
                        'tmp', 'yt-info', videoId, JSON.stringify(info)
                    );
                }

                this.lastInfo = info;
                debug('video info is ready to use');

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

    async getPlaybackDataAsync(videoId)
    {
        const info = await this.getVideoInfoPromise(videoId).catch(debug);

        if(!info)
            throw new Error('no YouTube video info');

        let uri = null;
        const dashInfo = await this.getDashInfoAsync(info).catch(debug);

        if(dashInfo) {
            debug('parsed video info to dash info');
            const dash = Dash.generateDash(dashInfo);

            if(dash) {
                debug('got dash data');

                const dashFile = await FileOps.saveFilePromise(
                    'tmp', null, 'clapper.mpd', dash
                ).catch(debug);

                if(dashFile)
                    uri = dashFile.get_uri();

                debug('got dash file');
            }
        }

        if(!uri)
            uri = this.ytClient.getBestCombinedUri(info);

        if(!uri)
            throw new Error('no YouTube video URI');

        debug(`final URI: ${uri}`);

        const title = (info.videoDetails && info.videoDetails.title)
            ? Misc.decodeURIPlus(info.videoDetails.title)
            : videoId;

        debug(`title: ${title}`);

        return { uri, title };
    }

    async getDashInfoAsync(info)
    {
        if(
            !info.streamingData
            || !info.streamingData.adaptiveFormats
            || !info.streamingData.adaptiveFormats.length
        )
            return null;

        /* TODO: Options in prefs to set preferred video formats for adaptive streaming */
        const videoStream = info.streamingData.adaptiveFormats.find(stream => {
            return (stream.mimeType.startsWith('video/mp4') && stream.quality === 'hd1080');
        });
        const audioStream = info.streamingData.adaptiveFormats.find(stream => {
            return (stream.mimeType.startsWith('audio/mp4'));
        });

        if(!videoStream || !audioStream)
            return null;

        debug('following redirects');

        for(let stream of [videoStream, audioStream]) {
            debug(`initial URL: ${stream.url}`);

            const result = await this._downloadDataPromise(stream.url, 'HEAD').catch(debug);
            if(!result) return null;

            stream.url = result.uri;
            debug(`resolved URL: ${stream.url}`);
        }

        debug('all redirects resolved');

        return {
            duration: info.videoDetails.lengthSeconds,
            adaptations: [
                [videoStream],
                [audioStream],
            ]
        };
    }

    getBestCombinedUri(info)
    {
        debug('obtaining best combined URL');

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

    _downloadDataPromise(url, method, reqData)
    {
        method = method || 'GET';

        return new Promise((resolve, reject) => {
            const message = Soup.Message.new(method, url);
            const result = {
                data: null,
                isAborted: false,
                uri: null,
            };

            if(reqData) {
                message.set_request(
                    "application/json",
                    Soup.MemoryUse.COPY,
                    reqData
                );
            }

            this.queue_message(message, (session, msg) => {
                debug('got message response');
                const statusCode = msg.status_code;

                if(statusCode === 200) {
                    result.data = msg.response_body.data;

                    if(method === 'HEAD')
                        result.uri = msg.uri.to_string(false);

                    return resolve(result);
                }

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

    _getPlayabilityStatus(info)
    {
        if(
            !info.playabilityStatus
            || !info.playabilityStatus.status === 'OK'
        )
            return [false, 'video is not playable'];

        if(!info.streamingData)
            return [false, 'video response data is missing streaming data'];

        return [true, null];
    }

    _getReducedInfo(info)
    {
        const reduced = {
            videoDetails: {
                videoId: info.videoDetails.videoId,
                title: info.videoDetails.title,
                lengthSeconds: info.videoDetails.lengthSeconds,
                isLiveContent: info.videoDetails.isLiveContent
            },
            streamingData: info.streamingData
        };

        /* Make sure we have all formats arrays,
         * so we will not have to keep checking */
        if(!reduced.streamingData.formats)
            reduced.streamingData.formats = [];
        if(!reduced.streamingData.adaptiveFormats)
            reduced.streamingData.adaptiveFormats = [];

        return reduced;
    }

    _getPlayerInfoPromise(videoId)
    {
        const data = this._getPlayerPostData(videoId);
        const apiKey = 'AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8';
        const url = `https://www.youtube.com/youtubei/v1/player?key=${apiKey}`;

        return new Promise((resolve, reject) => {
            if(!data) {
                debug('not using player info due to missing data');
                return resolve(null);
            }
            debug('downloading info from player');

            this._downloadDataPromise(url, 'POST', data).then(result => {
                if(result.isAborted)
                    return resolve(result);

                debug('parsing player info JSON');

                let info = null;

                try { info = JSON.parse(result.data); }
                catch(err) { debug(err.message); }

                if(!info)
                    return reject(new Error('could not parse video info JSON'));

                debug('successfully parsed video info JSON');

                /* Update post info values from response */
                if(info && info.responseContext && info.responseContext.visitorData) {
                    const visData = info.responseContext.visitorData;

                    this.postInfo.visitorData = visData;
                    debug(`new visitor ID: ${visData}`);
                }

                result.data = info;
                resolve(result);
            })
            .catch(err => reject(err));
        });
    }

    _getInfoPromise(videoId)
    {
        return new Promise((resolve, reject) => {
            const query = [
                `video_id=${videoId}`,
                `el=embedded`,
                `eurl=https://youtube.googleapis.com/v/${videoId}`,
                `sts=${this.cachedSig.timestamp}`,
            ].join('&');
            const url = `https://www.youtube.com/get_video_info?${query}`;

            debug('downloading info from video');

            this._downloadDataPromise(url).then(result => {
                if(result.isAborted)
                    return resolve(result);

                debug('parsing video info JSON');

                const gstUri = Gst.Uri.from_string('?' + result.data);

                if(!gstUri)
                    return reject(new Error('could not convert query to URI'));

                const playerResponse = gstUri.get_query_value('player_response');
                const cliVer = gstUri.get_query_value('cver');

                if(cliVer && cliVer !== this.postInfo.clientVersion) {
                    this.postInfo.clientVersion = cliVer;
                    debug(`updated client version: ${cliVer}`);
                }

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

                /* Remove unneeded data */
                if(stream.signatureCipher)
                    delete stream.signatureCipher;
                if(stream.cipher)
                    delete stream.cipher;
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

        return `${url}&${sig}=${encodeURIComponent(key)}`;
    }

    _getPlayerPostData(videoId)
    {
        const cliVer = this.postInfo.clientVersion;
        if(!cliVer) return null;

        const visitor = this.postInfo.visitorData;
        const sts = this.cachedSig.timestamp || null;

        const ua = this.user_agent;
        const browserVer = ua.substring(ua.lastIndexOf('/') + 1);

        if(!visitor)
            debug('visitor ID is unknown');

        const data = {
            videoId: videoId,
            context: {
                client: {
                    visitorData: visitor,
                    userAgent: `${ua},gzip(gfe)`,
                    clientName: "WEB",
                    clientVersion: cliVer,
                    osName: "X11",
                    osVersion: "",
                    originalUrl: `https://www.youtube.com/watch?v=${videoId}`,
                    browserName: "Firefox",
                    browserVersion: browserVer,
                    playerType: "UNIPLAYER"
                },
                user: {
                    lockedSafetyMode: false
                },
                request: {
                    useSsl: true,
                    internalExperimentFlags: [],
                    consistencyTokenJars: []
                }
            },
            playbackContext: {
                contentPlaybackContext: {
                    html5Preference: "HTML5_PREF_WANTS",
                    lactMilliseconds: "1069",
                    referer: `https://www.youtube.com/watch?v=${videoId}`,
                    signatureTimestamp: sts,
                    autoCaptionsDefaultOn: false,
                    liveContext: {
                        startWalltime: "0"
                    }
                }
            },
            captionParams: {}
        };

        return JSON.stringify(data);
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
