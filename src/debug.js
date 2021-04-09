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

function _debug(msg, debuggerName)
{
    if(msg.message) {
        GLib.log_structured(
            debuggerName, GLib.LogLevelFlags.LEVEL_CRITICAL, {
                MESSAGE: msg.message,
                SYSLOG_IDENTIFIER: debuggerName.toLowerCase()
        });

        return;
    }

    switch(debuggerName) {
        case 'Clapper':
            clapperDebugger.debug(msg);
            break;
        case 'YouTube':
            ytDebugger.debug(msg);
            break;
    }
}

function debug(msg)
{
    _debug(msg, 'Clapper');
}

function ytDebug(msg)
{
    _debug(msg, 'YouTube');
}
