const { Gio, GLib } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

function generateDash(info)
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

    const bufferSec = Math.min(4, info.videoDetails.lengthSeconds);

    return [
        `<?xml version="1.0" encoding="UTF-8"?>`,
        `<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"`,
        `  xmlns="urn:mpeg:dash:schema:mpd:2011"`,
        `  xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd"`,
        `  type="static"`,
        `  mediaPresentationDuration="PT${info.videoDetails.lengthSeconds}S"`,
        `  minBufferTime="PT${bufferSec}S"`,
        `  profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">`,
        `  <Period>`,
             _addAdaptationSet([videoStream]),
             _addAdaptationSet([audioStream]),
        `  </Period>`,
        `</MPD>`
    ].join('\n');
}

function saveDashPromise(dash)
{
    return new Promise((resolve, reject) => {
        const tempPath = GLib.get_tmp_dir() + '/.clapper.mpd';
        const dashFile = Gio.File.new_for_path(tempPath);

        debug('saving dash file');

        dashFile.replace_contents_bytes_async(
            GLib.Bytes.new_take(dash),
            null,
            false,
            Gio.FileCreateFlags.NONE,
            null
        )
        .then(() => {
            debug('saved dash file');
            resolve(dashFile.get_uri());
        })
        .catch(err => reject(err));
    });
}

function _addAdaptationSet(streamsArr)
{
    const mimeInfo = _getMimeInfo(streamsArr[0].mimeType);

    const adaptArr = [
        `contentType="${mimeInfo.content}"`,
        `mimeType="${mimeInfo.type}"`,
        `subsegmentAlignment="true"`,
        `subsegmentStartsWithSAP="1"`,
    ];

    const widthArr = [];
    const heightArr = [];
    const fpsArr = [];

    const representations = [];

    for(let stream of streamsArr) {
        /* No point parsing if no URL */
        if(!stream.url)
            continue;

        if(stream.width && stream.height) {
            widthArr.push(stream.width);
            heightArr.push(stream.height);
        }
        if(stream.fps)
            fpsArr.push(stream.fps);

        representations.push(_getStreamRepresentation(stream));
    }

    if(widthArr.length && heightArr.length) {
        const maxWidth = Math.max.apply(null, widthArr);
        const maxHeight = Math.max.apply(null, heightArr);
        const par = _getPar(maxWidth, maxHeight);

        adaptArr.push(`maxWidth="${maxWidth}"`);
        adaptArr.push(`maxHeight="${maxHeight}"`);
        adaptArr.push(`par="${par}"`);
    }
    if(fpsArr.length) {
        const maxFps = Math.max.apply(null, fpsArr);

        adaptArr.push(`maxFrameRate="${maxFps}"`);
    }

    const adaptationSet = [
        `    <AdaptationSet ${adaptArr.join(' ')}>`,
               representations.join('\n'),
        `    </AdaptationSet>`
    ];

    return adaptationSet.join('\n');
}

function _getStreamRepresentation(stream)
{
    const mimeInfo = _getMimeInfo(stream.mimeType);

    const repOptsArr = [
        `id="${stream.itag}"`,
        `codecs="${mimeInfo.codecs}"`,
        `bandwidth="${stream.bitrate}"`,
    ];

    if(stream.width && stream.height) {
        repOptsArr.push(`width="${stream.width}"`);
        repOptsArr.push(`height="${stream.height}"`);
        repOptsArr.push(`sar="1:1"`);
    }
    if(stream.fps)
        repOptsArr.push(`frameRate="${stream.fps}"`);

    const repArr = [
        `      <Representation ${repOptsArr.join(' ')}>`,
    ];
    if(stream.audioChannels) {
        const audioConfArr = [
            `schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011"`,
            `value="${stream.audioChannels}"`,
        ];
        repArr.push(`      <AudioChannelConfiguration ${audioConfArr.join(' ')}/>`);
    }

    const encodedURL = Misc.encodeHTML(stream.url);
    const segRange = `${stream.indexRange.start}-${stream.indexRange.end}`;
    const initRange = `${stream.initRange.start}-${stream.initRange.end}`;

    repArr.push(
        `        <BaseURL>${encodedURL}</BaseURL>`,
        `<!-- FIXME: causes string query omission bug in dashdemux`,
        `        <SegmentBase indexRange="${segRange}">`,
        `          <Initialization range="${initRange}"/>`,
        `        </SegmentBase>`,
        `-->`,
        `      </Representation>`,
    );

    return repArr.join('\n');
}

function _getMimeInfo(mimeType)
{
    const mimeArr = mimeType.split(';');

    let codecs = mimeArr.find(info => info.includes('codecs')).split('=')[1];
    codecs = codecs.substring(1, codecs.length - 1);

    const mimeInfo = {
        content: mimeArr[0].split('/')[0],
        type: mimeArr[0],
        codecs,
    };

    return mimeInfo;
}

function _getPar(width, height)
{
    const gcd = _getGCD(width, height);

    width /= gcd;
    height /= gcd;

    return `${width}:${height}`;
}

function _getGCD(width, height)
{
    return (height)
        ? _getGCD(height, width % height)
        : width;
}
