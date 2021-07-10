const { GObject, Gst, Soup } = imports.gi;
const Dash = imports.src.dash;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;
const YTItags = imports.src.youtubeItags;
const YTDL = imports.src.assets['node-ytdl-core'];

const debug = Debug.ytDebug;
const { settings } = Misc;

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
            clientVersion: "2.20210605.09.00",
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

    async getPlaybackDataAsync(videoId, monitor)
    {
        const info = await this.getVideoInfoPromise(videoId).catch(debug);

        if(!info)
            throw new Error('no YouTube video info');

        let uri = null;
        const itagOpts = {
            width: monitor.geometry.width * monitor.scale_factor,
            height: monitor.geometry.height * monitor.scale_factor,
            codec: 'h264',
            type: YTItags.QualityType[settings.get_int('yt-quality-type')],
            adaptive: settings.get_boolean('yt-adaptive-enabled'),
        };

        uri = await this.getHLSUriAsync(info, itagOpts);

        if(!uri) {
            const dashInfo = await this.getDashInfoAsync(info, itagOpts).catch(debug);

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
        }

        if(!uri)
            uri = this.getBestCombinedUri(info, itagOpts);

        if(!uri)
            throw new Error('no YouTube video URI');

        debug(`final URI: ${uri}`);

        const title = (info.videoDetails && info.videoDetails.title)
            ? Misc.decodeURIPlus(info.videoDetails.title)
            : videoId;

        debug(`title: ${title}`);

        return { uri, title };
    }

    async getHLSUriAsync(info, itagOpts)
    {
        const isLive = (
            info.videoDetails.isLiveContent
            && (!info.videoDetails.lengthSeconds
            || Number(info.videoDetails.lengthSeconds) <= 0)
        );
        debug(`video is live: ${isLive}`);

        /* YouTube only uses HLS for live content */
        if(!isLive)
            return null;

        const hlsUri = info.streamingData.hlsManifestUrl;
        if(!hlsUri) {
            /* HLS may be unavailable on finished live streams */
            debug('no HLS manifest URL');
            return null;
        }

        if(!itagOpts.adaptive) {
            const result = await this._downloadDataPromise(hlsUri).catch(debug);
            if(!result || !result.data) {
                debug(new Error('HLS manifest download failed'));
                return hlsUri;
            }

            const hlsArr = result.data.split('\n');
            const hlsStreams = [];

            let index = hlsArr.length;
            while(index--) {
                const url = hlsArr[index];
                if(!Gst.Uri.is_valid(url))
                    continue;

                const itagIndex = url.indexOf('/itag/') + 6;
                const itag = url.substring(itagIndex, itagIndex + 2);

                hlsStreams.push({ itag, url });
            }

            debug(`obtaining HLS itags for resolution: ${itagOpts.width}x${itagOpts.height}`);
            const hlsItags = YTItags.getHLSItags(itagOpts);
            debug(`HLS itags: ${JSON.stringify(hlsItags)}`);

            const hlsStream = this.getBestStreamFromItags(hlsStreams, hlsItags);
            if(hlsStream)
                return hlsStream.url;
        }

        return hlsUri;
    }

    async getDashInfoAsync(info, itagOpts)
    {
        if(
            !info.streamingData
            || !info.streamingData.adaptiveFormats
            || !info.streamingData.adaptiveFormats.length
        )
            return null;

        debug(`obtaining DASH itags for resolution: ${itagOpts.width}x${itagOpts.height}`);
        const dashItags = YTItags.getDashItags(itagOpts);
        debug(`DASH itags: ${JSON.stringify(dashItags)}`);

        const filteredStreams = {
            video: [],
            audio: [],
        };

        for(let fmt of ['video', 'audio']) {
            debug(`filtering ${fmt} streams`);
            let index = dashItags[fmt].length;

            while(index--) {
                const itag = dashItags[fmt][index];
                const foundStream = info.streamingData.adaptiveFormats.find(stream => stream.itag == itag);
                if(foundStream) {
                    /* Parse and convert mimeType string into object */
                    foundStream.mimeInfo = this._getMimeInfo(foundStream.mimeType);

                    /* Sanity check */
                    if(!foundStream.mimeInfo || foundStream.mimeInfo.content !== fmt) {
                        debug(new Error(`mimeType parsing failed on stream: ${itag}`));
                        continue;
                    }

                    /* Sort from worst to best */
                    filteredStreams[fmt].unshift(foundStream);
                    debug(`added ${fmt} itag: ${foundStream.itag}`);

                    if(!itagOpts.adaptive)
                        break;
                }
            }
            if(!filteredStreams[fmt].length) {
                debug(`dash info ${fmt} streams list is empty`);
                return null;
            }
        }

        debug('following redirects');

        for(let fmtArr of Object.values(filteredStreams)) {
            for(let stream of fmtArr) {
                debug(`initial URL: ${stream.url}`);

                /* Errors in some cases are to be expected here,
                 * so be quiet about them and use fallback methods */
                const result = await this._downloadDataPromise(
                    stream.url, 'HEAD'
                ).catch(err => debug(err.message));

                if(!result || !result.uri) {
                    debug('redirect could not be resolved');
                    return null;
                }

                stream.url = Misc.encodeHTML(result.uri)
                    .replace('?', '/')
                    .replace(/&amp;/g, '/')
                    .replace(/=/g, '/');

                debug(`resolved URL: ${stream.url}`);
            }
        }

        debug('all redirects resolved');

        return {
            duration: info.videoDetails.lengthSeconds,
            adaptations: [
                filteredStreams.video,
                filteredStreams.audio,
            ]
        };
    }

    getBestCombinedUri(info, itagOpts)
    {
        debug(`obtaining best combined URL for resolution: ${itagOpts.width}x${itagOpts.height}`);
        const streams = info.streamingData.formats;

        if(!streams.length)
            return null;

        const combinedItags = YTItags.getCombinedItags(itagOpts);
        let combinedStream = this.getBestStreamFromItags(streams, combinedItags);

        if(!combinedStream) {
            debug('trying any combined stream as last resort');
            combinedStream = streams[streams.length - 1];
        }

        if(!combinedStream || !combinedStream.url)
            return null;

        return combinedStream.url;
    }

    getBestStreamFromItags(streams, itags)
    {
        let index = itags.length;

        while(index--) {
            const itag = itags[index];
            const stream = streams.find(stream => stream.itag == itag);
            if(stream) {
                debug(`found preferred stream itag: ${stream.itag}`);
                return stream;
            }
        }
        debug('could not find preferred stream for itags');

        return null;
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

                debug(`response code: ${statusCode}`);

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

    _getMimeInfo(mimeType)
    {
        debug(`parsing mimeType: ${mimeType}`);

        const mimeArr = mimeType.split(';');

        let codecs = mimeArr.find(info => info.includes('codecs')).split('=')[1];
        codecs = codecs.substring(1, codecs.length - 1);

        const mimeInfo = {
            content: mimeArr[0].split('/')[0],
            type: mimeArr[0],
            codecs,
        };

        debug(`parsed mimeType: ${JSON.stringify(mimeInfo)}`);

        return mimeInfo;
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
                `html5=1`,
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
        const stream = (data.formats.length)
            ? data.formats[0]
            : data.adaptiveFormats[0];

        if(!stream) {
            debug(new Error('no streams'));
            return false;
        }

        if(stream.url)
            return false;

        if(
            stream.signatureCipher
            || stream.cipher
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
        case 'm.youtube.com':
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
