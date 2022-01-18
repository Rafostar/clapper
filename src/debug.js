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

    clapperDebugger.debug(msg);
}

function debug(msg)
{
    _debug('Clapper', msg);
}

function warn(msg)
{
    _logStructured('Clapper', msg, GLib.LogLevelFlags.LEVEL_WARNING);
}
