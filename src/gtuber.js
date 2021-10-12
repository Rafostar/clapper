const { Gio, GstClapper } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;
const FileOps = imports.src.fileOps;
const Gtuber = Misc.tryImport('Gtuber');

const { debug, warn } = Debug;
const { settings } = Misc;

const best = {
    video: null,
    audio: null,
    video_audio: null,
};
const codecPairs = [];
const qualityType = {
    0: 30, // normal
    1: 60, // hfr
};

var isAvailable = (Gtuber != null);
var cancellable = null;
let client = null;

function resetBestStreams()
{
    best.video = null;
    best.audio = null;
    best.video_audio = null;
}

function isStreamAllowed(stream, opts)
{
    const vcodec = stream.video_codec;
    const acodec = stream.audio_codec;

    if(
        vcodec
        && (!vcodec.startsWith(opts.vcodec)
        || (stream.height < 240 || stream.height > opts.height)
        || stream.fps > qualityType[opts.quality])
    ) {
        return false;
    }

    if(
        acodec
        && (!acodec.startsWith(opts.acodec))
    ) {
        return false;
    }

    return (vcodec != null || acodec != null);
}

function updateBestStreams(streams, opts)
{
    for(let stream of streams) {
        if(!isStreamAllowed(stream, opts))
            continue;

        const type = (stream.video_codec && stream.audio_codec)
            ? 'video_audio'
            : (stream.video_codec)
            ? 'video'
            : 'audio';

        if(!best[type] || best[type].bitrate < stream.bitrate)
            best[type] = stream;
    }
}

function _streamFilter(opts, stream)
{
    switch(stream) {
        case best.video:
            return (best.audio != null || best.video_audio == null);
        case best.audio:
            return (best.video != null || best.video_audio == null);
        case best.video_audio:
            return (best.video == null || best.audio == null);
        default:
            return (opts.adaptive)
                ? isStreamAllowed(stream, opts)
                : false;
    }
}

function generateManifest(info, opts)
{
    const gen = new Gtuber.ManifestGenerator({
        pretty: Debug.enabled,
    });
    gen.set_media_info(info);
    gen.set_filter_func(_streamFilter.bind(this, opts));

    debug('trying to get manifest');

    for(let pair of codecPairs) {
        opts.vcodec = pair[0];
        opts.acodec = pair[1];

        /* Find best streams among adaptive ones */
        if (!opts.adaptive)
            updateBestStreams(info.get_adaptive_streams(), opts);

        const data = gen.to_data();

        /* Release our ref */
        if (!opts.adaptive)
            resetBestStreams();

        if(data) {
            debug('got manifest');
            return data;
        }
    }

    debug('manifest not generated');

    return null;
}

function getBestCombinedUri(info, opts)
{
    const streams = info.get_streams();

    debug('searching for best combined URI');

    for(let pair of codecPairs) {
        opts.vcodec = pair[0];
        opts.acodec = pair[1];

        /* Find best non-adaptive stream */
        updateBestStreams(streams, opts);

        const bestUri = (best.video_audio)
            ? best.video_audio.get_uri()
            : (best.audio)
            ? best.audio.get_uri()
            : (best.video)
            ? best.video.get_uri()
            : null;

        /* Release our ref */
        resetBestStreams();

        if(bestUri) {
            debug('got best possible URI');
            return bestUri;
        }
    }

    /* If still nothing find stream by height */
    for(let stream of streams) {
        const height = stream.get_height();
        if(!height || height > opts.height)
            continue;

        if(!best.video_audio || best.video_audio.height < stream.height)
            best.video_audio = stream;
    }

    const anyUri = (best.video_audio)
        ? best.video_audio.get_uri()
        : null;

    /* Release our ref */
    resetBestStreams();

    if (anyUri)
        debug('got any URI');

    return anyUri;
}

async function _parseMediaInfoAsync(info, player)
{
    const resp = {
        uri: null,
        title: info.title,
    };

    const { root } = player.widget;
    const surface = root.get_surface();
    const monitor = root.display.get_monitor_at_surface(surface);

    const opts = {
        width: monitor.geometry.width * monitor.scale_factor,
        height: monitor.geometry.height * monitor.scale_factor,
        vcodec: null,
        acodec: null,
        quality: settings.get_int('yt-quality-type'),
        adaptive: settings.get_boolean('yt-adaptive-enabled'),
    };

    if(info.has_adaptive_streams) {
        const data = generateManifest(info, opts);
        if(data) {
            const manifestFile = await FileOps.saveFilePromise(
                'tmp', null, 'manifest', data
            ).catch(debug);

            if(!manifestFile)
                throw new Error('Gtuber: no manifest file was generated');

            resp.uri = manifestFile.get_uri();

            return resp;
        }
    }

    resp.uri = getBestCombinedUri(info, opts);

    if(!resp.uri)
        throw new Error("Gtuber: no compatible stream found");

    return resp;
}

function _createClient(player)
{
    client = new Gtuber.Client();
    debug('created new gtuber client');

    /* TODO: config based on what HW supports */
    //codecPairs.push(['vp9', 'opus']);

    codecPairs.push(['avc', 'mp4a']);
}

function mightHandleUri(uri)
{
    const unsupported = [
        'file', 'fd', 'dvd', 'cdda',
        'dvb', 'v4l2', 'gs'
    ];
    return !unsupported.includes(Misc.getUriProtocol(uri));
}

function cancelFetching()
{
    if(cancellable && !cancellable.is_cancelled())
        cancellable.cancel();
}

function parseUriPromise(uri, player)
{
    return new Promise((resolve, reject) => {
        if(!client) {
            if(!isAvailable) {
                debug('gtuber is not installed');
                return resolve({ uri, title: null });
            }
            _createClient(player);
        }

        /* Stop to show reaction and restore internet bandwidth */
        if(player.state !== GstClapper.ClapperState.STOPPED)
            player.stop();

        cancellable = new Gio.Cancellable();
        debug('gtuber is fetching media info...');

        client.fetch_media_info_async(uri, cancellable, (client, task) => {
            cancellable = null;
            let info = null;

            try {
                info = client.fetch_media_info_finish(task);
                debug('gtuber successfully fetched media info');
            }
            catch(err) {
                const taskCancellable = task.get_cancellable();

                if(taskCancellable.is_cancelled())
                    return reject(err);

                const gtuberNoPlugin = (
                    err.domain === Gtuber.ClientError.quark()
                    && err.code === Gtuber.ClientError.NO_PLUGIN
                );
                if(!gtuberNoPlugin)
                    return reject(err);

                warn(`Gtuber: ${err.message}, trying URI as is...`);

                /* Allow handling URI as is via GStreamer plugins */
                return resolve({ uri, title: null });
            }

            _parseMediaInfoAsync(info, player)
                .then(resp => resolve(resp))
                .catch(err => reject(err));
        });
    });
}
