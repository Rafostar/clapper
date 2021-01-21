const { GLib } = imports.gi;
const { Debug } = imports.extras.debug;
const { Ink } = imports.extras.ink;

const G_DEBUG_ENV = GLib.getenv('G_MESSAGES_DEBUG');

const clapperDebugger = new Debug.Debugger('Clapper', {
    name_printer: new Ink.Printer({
        font: Ink.Font.BOLD,
        color: Ink.Color.MAGENTA
    }),
    time_printer: new Ink.Printer({
        color: Ink.Color.ORANGE
    }),
    high_precision: true,
});
clapperDebugger.enabled = (
    clapperDebugger.enabled
    || G_DEBUG_ENV != null
    && G_DEBUG_ENV.includes('Clapper')
);
const clapperDebug = clapperDebugger.debug;

function debug(msg, levelName)
{
    levelName = levelName || 'LEVEL_DEBUG';

    if(msg.message) {
        levelName = 'LEVEL_CRITICAL';
        msg = msg.message;
    }

    if(levelName !== 'LEVEL_CRITICAL')
        return clapperDebug(msg);

    GLib.log_structured(
        'Clapper', GLib.LogLevelFlags[levelName], {
            MESSAGE: msg,
            SYSLOG_IDENTIFIER: 'clapper'
    });
}
