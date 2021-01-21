const { Soup } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.src.debug;

const { debug } = Debug;

function parseData(dataType, bytes)
{
    if(dataType !== Soup.WebsocketDataType.TEXT) {
        debug('ignoring non-text WebSocket message');
        return [false];
    }

    let parsedMsg = null;
    const msg = bytes.get_data();

    try {
        parsedMsg = JSON.parse(ByteArray.toString(msg));
    }
    catch(err) {
        debug(err);
    }

    if(!parsedMsg || !parsedMsg.action) {
        debug('no "action" in parsed WebSocket message');
        return [false];
    }

    return [true, parsedMsg];
}
