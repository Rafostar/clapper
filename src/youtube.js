const { GLib, GObject, Gst, Soup } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;

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

        this.downloadAborted = false;
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

                const info = await this._getInfoPromise(videoId).catch(debug);
                if(!info) {
                    if(this.downloadAborted)
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
                    ? 'video response data is missing URIs'
                    : null;

                if(invalidInfoMsg) {
                    this.lastInfo = null;
                    this.emit('info-resolved', false);
                    this.downloadingVideoId = null;

                    return reject(new Error(invalidInfoMsg));
                }

                this.lastInfo = info;
                this.emit('info-resolved', true);
                this.downloadingVideoId = null;

                return resolve(info);
            }

            /* Do not clear video info here, as we still have
             * valid info from last video that can be reused */
            this.emit('info-resolved', false);
            this.downloadingVideoId = null;

            reject(new Error('could not obtain YouTube video info'));
        });
    }

    getBestCombinedUri(info)
    {
        if(
            !info.streamingData.formats
            || !info.streamingData.formats.length
        )
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
            const url = `https://www.youtube.com/get_video_info?video_id=${videoId}&el=embedded`;
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

                /* Internal Soup codes mean download abort
                 * or some other error that cannot be handled */
                this.downloadAborted = (statusCode < 10);

                if(statusCode !== 200)
                    return reject(new Error(`response code: ${statusCode}`));

                debug('parsing video info JSON');

                const gstUri = Gst.Uri.from_string('?' + data);

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

                resolve(info);
            });
        });
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
