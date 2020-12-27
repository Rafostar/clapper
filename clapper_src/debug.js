const { GLib } = imports.gi;

function debug(msg, levelName)
{
    levelName = levelName || 'LEVEL_DEBUG';

    if(msg.message) {
        levelName = 'LEVEL_CRITICAL';
        msg = msg.message;
    }
    GLib.log_structured(
        'Clapper', GLib.LogLevelFlags[levelName], {
            MESSAGE: msg,
            SYSLOG_IDENTIFIER: 'clapper'
    });
}
