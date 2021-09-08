const Debug = imports.debug;
const FileOps = imports.fileOps;
const Misc = imports.misc;

const { debug } = Debug;

function generateDash(dashInfo)
{
    debug('generating dash');

    const bufferSec = Math.min(4, dashInfo.duration);

    const dash = [
        `<?xml version="1.0" encoding="UTF-8"?>`,
        `<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"`,
        `  xmlns="urn:mpeg:dash:schema:mpd:2011"`,
        `  xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd"`,
        `  type="static"`,
        `  mediaPresentationDuration="PT${dashInfo.duration}S"`,
        `  minBufferTime="PT${bufferSec}S"`,
        `  profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">`,
        `  <Period>`
    ];

    for(let adaptation of dashInfo.adaptations)
        dash.push(_addAdaptationSet(adaptation));

    dash.push(
        `  </Period>`,
        `</MPD>`
    );

    debug('dash generated');

    return dash.join('\n');
}

function _addAdaptationSet(streamsArr)
{
    /* We just need it for adaptation type,
     * so any stream will do */
    const { mimeInfo } = streamsArr[0];

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
    const repOptsArr = [
        `id="${stream.itag}"`,
        `codecs="${stream.mimeInfo.codecs}"`,
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

    repArr.push(
        `        <BaseURL>${stream.url}</BaseURL>`
    );

    if(stream.indexRange) {
        const segRange = `${stream.indexRange.start}-${stream.indexRange.end}`;
        repArr.push(
            `        <SegmentBase indexRange="${segRange}">`
        );
        if(stream.initRange) {
            const initRange = `${stream.initRange.start}-${stream.initRange.end}`;
            repArr.push(
                `          <Initialization range="${initRange}"/>`
            );
        }
        repArr.push(
            `        </SegmentBase>`
        );
    }

    repArr.push(
        `      </Representation>`
    );

    return repArr.join('\n');
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
