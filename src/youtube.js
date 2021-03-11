const { GLib, GObject, Gst, Soup } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;

const { debug } = Debug;

var YouTubeClient = GObject.registerClass(
class ClapperYouTubeClient extends Soup.Session
{
    _init()
    {
        super._init({
            timeout: 5,
        });
    }

    getVideoInfoPromise(videoId)
    {
        return new Promise(async (resolve, reject) => {
            const url = `https://www.youtube.com/get_video_info?video_id=${videoId}&el=embedded`;
            let tries = 2;

            while(tries--) {
                debug(`obtaining YouTube video info: ${videoId}`);

                const info = await this._getInfoPromise(url).catch(debug);
                if(!info) {
                    debug(`failed, remaining tries: ${tries}`);
                    continue;
                }

                /* Check if video is playable */
                if(
                    !info.playabilityStatus
                    || !info.playabilityStatus.status === 'OK'
                )
                    return reject(new Error('video is not playable'));

                /* Check if data contains streaming URIs */
                if(!info.streamingData)
                    return reject(new Error('video response data is missing URIs'));

                return resolve(info);
            }

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

    _getInfoPromise(url)
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

                if(msg.status_code !== 200)
                    return reject(new Error(`response code: ${msg.status_code}`));

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
