const { GLib } = imports.gi;

let ink = { Ink: null };
try {
    ink = imports.ink;
} catch(e) {}
const { Ink } = ink;

const DEBUG_ENV = GLib.getenv('DEBUG');

var Debugger = class
{
    constructor(name, opts)
    {
        opts = (opts && typeof opts === 'object')
            ? opts : {};

        this.name = (name && typeof name === 'string')
            ? name : 'GJS';

        this.print_state = (opts.print_state)
            ? true : false;

        this.json_space = (typeof opts.json_space === 'number')
            ? opts.json_space : 2;

        this.name_printer    = opts.name_printer    || this._getInkPrinter(true);
        this.message_printer = opts.message_printer || this._getDefaultPrinter();
        this.time_printer    = opts.time_printer    || this._getInkPrinter();
        this.high_precision  = opts.high_precision  || false;

        if(typeof opts.color !== 'undefined')
            this.color = opts.color;

        this._isEnabled = false;
        this._lastDebug = GLib.get_monotonic_time();

        this.enabled = (typeof opts.enabled !== 'undefined')
            ? opts.enabled : this._enabledAtStart;
    }

    get enabled()
    {
        return this._isEnabled;
    }

    set enabled(value)
    {
        if(this._isEnabled === value)
            return;

        this._isEnabled = (value) ? true : false;

        if(!this.print_state)
            return;

        let state = (this.enabled) ? 'en' : 'dis';
        this._runDebug(`debug ${state}abled`);
    }

    get color()
    {
        return this.name_printer.color;
    }

    set color(value)
    {
        this.name_printer.color = value;
        this.time_printer.color = this.name_printer.color;
    }

    get debug()
    {
        return message => this._debug(message);
    }

    get _enabledAtStart()
    {
        if(!DEBUG_ENV)
            return false;

        let envArr = DEBUG_ENV.split(',');

        return envArr.some(el => {
            if(el === this.name || el === '*')
                return true;

            let searchType;
            let offset = 0;

            if(el.startsWith('*')) {
                searchType = 'ends';
            } else if(el.endsWith('*')) {
                searchType = 'starts';
                offset = 1;
            }

            if(!searchType)
                return false;

            return this.name[searchType + 'With'](
                el.substring(1 - offset, el.length - offset)
            );
        });
    }

    _getInkPrinter(isBold)
    {
        if(!Ink)
            return this._getDefaultPrinter();

        let printer = new Ink.Printer({
            color: Ink.colorFromText(this.name)
        });

        if(isBold)
            printer.font = Ink.Font.BOLD;

        return printer;
    }

    _getDefaultPrinter()
    {
        return {
            getPainted: function() {
                return Object.values(arguments);
            }
        };
    }

    _debug(message)
    {
        if(!this.enabled)
            return;

        this._runDebug(message);
    }

    _runDebug(message)
    {
        switch(typeof message) {
            case 'string':
                break;
            case 'object':
                if(
                    message !== null
                    && (message.constructor === Object
                    || message.constructor === Array)
                ) {
                    message = JSON.stringify(message, null, this.json_space);
                    break;
                }
            default:
                message = String(message);
                break;
        }

        let time = GLib.get_monotonic_time() - this._lastDebug;

        if(!this.high_precision) {
            time = (time < 1000)
                ? '+0ms'
                : (time < 1000000)
                ? '+' + Math.floor(time / 1000) + 'ms'
                : '+' + Math.floor(time / 1000000) + 's';
        }
        else {
            time = (time < 1000)
                ? '+' + time + 'µs'
                : (time < 1000000)
                ? '+' + (time / 1000).toFixed(3) + 'ms'
                : '+' + (time / 1000000).toFixed(3) + 's';
        }

        printerr(
            this.name_printer.getPainted(this.name),
            this.message_printer.getPainted(message),
            this.time_printer.getPainted(time)
        );

        this._lastDebug = GLib.get_monotonic_time();
    }
}
