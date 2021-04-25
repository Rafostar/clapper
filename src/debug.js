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

function _logStructured(debuggerName, msg, level)
{
    GLib.log_structured(
        debuggerName, level, {
            MESSAGE: msg,
            SYSLOG_IDENTIFIER: debuggerName.toLowerCase()
    });
}

function _debug(debuggerName, msg)
{
    if(msg.message) {
        _logStructured(
            debuggerName,
            msg.message,
            GLib.LogLevelFlags.LEVEL_CRITICAL
        );

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
    _debug('Clapper', msg);
}

function ytDebug(msg)
{
    _debug('YouTube', msg);
}

function warn(msg)
{
    _logStructured('Clapper', msg, GLib.LogLevelFlags.LEVEL_WARNING);
}
