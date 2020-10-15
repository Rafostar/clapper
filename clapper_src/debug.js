const { GLib, Gst } = imports.gi;
const REQ_GST_VERSION_MINOR = 16;

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
    if(Gst.VERSION_MINOR >= REQ_GST_VERSION_MINOR)
        return;

    debug(
        'clapper interface was designed to' +
        ` work with GStreamer 1.${REQ_GST_VERSION_MINOR} or later.` +
        ` Your version is ${Gst.VERSION_MAJOR}.${Gst.VERSION_MINOR}.` +
        ' Please update GStreamer or expect some things to be broken.',
        'LEVEL_WARNING'
    );
}
