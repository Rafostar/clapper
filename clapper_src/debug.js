const { GLib, Gst } = imports.gi;

const GST_VERSION = Gst.version();
const REQ_GST_VER_MINOR = 16;

function debug(msg, levelName)
{
    levelName = levelName || 'LEVEL_DEBUG';

    if(msg instanceof Error) {
        levelName = 'LEVEL_CRITICAL';
        msg = msg.message;
    }
    GLib.log_structured(
        'Clapper', GLib.LogLevelFlags[levelName], {
            MESSAGE: msg,
            SYSLOG_IDENTIFIER: 'clapper'
    });
}

function gstVersionCheck()
{
    if(GST_VERSION[1] >= REQ_GST_VER_MINOR)
        return;

    debug(
        'clapper interface was designed to' +
        ` work with GStreamer 1.${REQ_GST_VER_MINOR} or later.` +
        ` Your version is ${GST_VERSION[0]}.${GST_VERSION[1]}.` +
        ' Please update GStreamer or expect some things to be broken.',
        'LEVEL_WARNING'
    );
}
