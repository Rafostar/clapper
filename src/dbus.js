const { Gio } = imports.gi;
const Debug = imports.src.debug;

const { debug } = Debug;

const ShellProxyWrapper = Gio.DBusProxy.makeProxyWrapper(`
<node>
  <interface name="org.gnome.Shell">
    <method name="Eval">
      <arg type="s" direction="in" name="script"/>
      <arg type="b" direction="out" name="success"/>
      <arg type="s" direction="out" name="result"/>
    </method>
  </interface>
</node>`
);

let shellProxy = new ShellProxyWrapper(
    Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell'
);

function shellWindowEval(fn, isEnabled)
{
    const un = (isEnabled) ? '' : 'un';

    debug(`changing ${fn}`);
    shellProxy.EvalRemote(
        `global.display.focus_window.${un}${fn}()`,
        (out) => {
            const debugMsg = (out[0])
                ? `window ${fn}: ${isEnabled}`
                : new Error(out[1]);

            debug(debugMsg);
        }
    );
}
