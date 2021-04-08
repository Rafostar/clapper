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

const ytDebugger = new Debug.Debugger('YouTube', {
    name_printer: new Ink.Printer({
        font: Ink.Font.BOLD,
        color: Ink.Color.RED
    }),
    time_printer: new Ink.Printer({
        color: Ink.Color.LIGHT_BLUE
    }),
    high_precision: true,
});

function _debug(msg, levelName, debuggerName)
{
    levelName = levelName || 'LEVEL_DEBUG';

    if(msg.message) {
        levelName = 'LEVEL_CRITICAL';
        msg = msg.message;
    }

    if(levelName !== 'LEVEL_CRITICAL') {
        switch(debuggerName) {
            case 'Clapper':
                clapperDebugger.debug(msg);
                break;
            case 'YouTube':
                ytDebugger.debug(msg);
                break;
        }

        return;
    }

    GLib.log_structured(
        debuggerName, GLib.LogLevelFlags[levelName], {
            MESSAGE: msg,
            SYSLOG_IDENTIFIER: debuggerName.toLowerCase()
    });
}

function debug(msg, levelName)
{
    _debug(msg, levelName, 'Clapper');
}

function ytDebug(msg, levelName)
{
    _debug(msg, levelName, 'YouTube');
}
