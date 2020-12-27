const TERM_ESC = '\x1B[';
const TERM_RESET = '0m';

var maxTransparency = 128;

var Font = {
    VARIOUS: null,
    REGULAR: 0,
    BOLD: 1,
    DIM: 2,
    ITALIC: 3,
    UNDERLINE: 4,
    BLINK: 5,
    REVERSE: 7,
    HIDDEN: 8,
    STRIKEOUT: 9,
};

var Color = {
    VARIOUS: null,
    DEFAULT: 39,
    BLACK: 30,
    RED: 31,
    GREEN: 32,
    YELLOW: 33,
    BLUE: 34,
    MAGENTA: 35,
    CYAN: 36,
    LIGHT_GRAY: 37,
    DARK_GRAY: 90,
    LIGHT_RED: 91,
    LIGHT_GREEN: 92,
    LIGHT_YELLOW: 93,
    LIGHT_BLUE: 94,
    LIGHT_MAGENTA: 95,
    LIGHT_CYAN: 96,
    WHITE: 97,
    BROWN: colorFrom256(52),
    LIGHT_BROWN: colorFrom256(130),
    PINK: colorFrom256(205),
    LIGHT_PINK: colorFrom256(211),
    ORANGE: colorFrom256(208),
    LIGHT_ORANGE: colorFrom256(214),
    SALMON: colorFrom256(209),
    LIGHT_SALMON: colorFrom256(216),
};

function colorFrom256(number)
{
    if(typeof number === 'undefined')
        number = Math.floor(Math.random() * 256) + 1;

    return `38;5;${number || 0}`;
}

function colorFromRGB(R, G, B, A)
{
    if(typeof R === 'undefined') {
        R = Math.floor(Math.random() * 256);
        G = Math.floor(Math.random() * 256);
        B = Math.floor(Math.random() * 256);
    }
    else if(typeof G === 'undefined' && Array.isArray(R)) {
        A = (R.length > 3) ? R[3] : 255;
        B = (R.length > 2) ? R[2] : 0;
        G = (R.length > 1) ? R[1] : 0;
        R = (R.length > 0) ? R[0] : 0;
    }

    if(_getIsTransparent(A))
        return Color.DEFAULT;

    R = R || 0;
    G = G || 0;
    B = B || 0;

    return `38;2;${R};${G};${B}`;
}

function colorFromHex(R, G, B, A)
{
    if((Array.isArray(R)))
        R = R.join('');

    let str = (typeof G === 'undefined')
        ? String(R)
        : (typeof A !== 'undefined')
        ? String(R) + String(G) + String(B) + String(A)
        : (typeof B !== 'undefined')
        ? String(R) + String(G) + String(B)
        : String(R) + String(G);

    let offset = (str[0] === '#') ? 1 : 0;
    let alphaIndex = 6 + offset;

    while(str.length < alphaIndex)
        str += '0';

    A = (str.length > alphaIndex)
        ? parseInt(str.substring(alphaIndex, alphaIndex + 2), 16)
        : 255;
    str = str.substring(offset, alphaIndex);

    let colorInt = parseInt(str, 16);
    let u8arr = new Uint8Array(3);

    u8arr[2] = colorInt;
    u8arr[1] = colorInt >> 8;
    u8arr[0] = colorInt >> 16;

    return colorFromRGB(u8arr[0], u8arr[1], u8arr[2], A);
}

function colorFromText(text)
{
    let value = _stringToDec(text);

    /* Returns color from 1 to 221 every 10 */
    return colorFrom256((value % 23) * 10 + 1);
}

function fontFromText(text)
{
    let arr = Object.keys(Font);
    let value = _stringToDec(text);

    /* Return a font excluding first (null) */
    return Font[arr[value % (arr.length - 1) + 1]];
}

function _getIsImage(args)
{
    if(args.length !== 1)
        return false;

    let arg = args[0];
    let argType = (typeof arg);

    if(argType === 'string' || argType === 'number')
        return false;

    if(!Array.isArray(arg))
        return false;

    let depth = 2;
    while(depth--) {
        arg = arg[0];
        if(!Array.isArray(arg))
            return false;
    }

    return arg.some(val => val !== 'number');
}

function _getIsTransparent(A)
{
    return (typeof A !== 'undefined' && A <= maxTransparency);
}

function _stringToDec(str)
{
    str = str || '';

    let len = str.length;
    let total = 0;

    while(len--)
        total += Number(str.charCodeAt(len).toString(10));

    return total;
}

var Printer = class
{
    constructor(opts)
    {
        opts = opts || {};

        const defaults = {
            font: Font.REGULAR,
            color: Color.DEFAULT,
            background: Color.DEFAULT
        };

        for(let def in defaults) {
            this[def] = (typeof opts[def] !== 'undefined')
                ? opts[def] : defaults[def];
        }
    }

    print()
    {
        (_getIsImage(arguments))
            ? this._printImage(arguments[0], 'stdout')
            : print(this._getPaintedArgs(arguments));
    }

    printerr()
    {
        (_getIsImage(arguments))
            ? this._printImage(arguments[0], 'stderr')
            : printerr(this._getPaintedArgs(arguments));
    }

    getPainted()
    {
        return (_getIsImage(arguments))
            ? this._printImage(arguments[0], 'return')
            : this._getPaintedArgs(arguments);
    }

    get background()
    {
        return this._background;
    }

    set background(value)
    {
        let valueType = (typeof value);

        if(valueType === 'string') {
            value = (value[2] === ';')
                ? '4' + value.substring(1)
                : Number(value);
        }
        this._background = (valueType === 'object')
            ? null
            : (value < 40 || value >= 90 && value < 100)
            ? value + 10
            : value;
    }

    _getPaintedArgs(args)
    {
        let str = '';

        for(let arg of args) {
            if(Array.isArray(arg))
                arg = arg.join(',');

            let painted = this._getPaintedString(arg);
            str += (str.length) ? ' ' + painted : painted;
        }

        return str;
    }

    _getPaintedString(text, noReset)
    {
        let str = TERM_ESC;

        for(let option of ['font', 'color', '_background']) {
            let optionType = (typeof this[option]);
            str += (optionType === 'number' || optionType === 'string')
                ? this[option]
                : (option === 'font' && Array.isArray(this[option]))
                ? this[option].join(';')
                : (option === 'font')
                ? fontFromText(text)
                : colorFromText(text);

            str += (option !== '_background') ? ';' : 'm';
        }
        str += text;

        return (noReset)
            ? str
            : (str + TERM_ESC + TERM_RESET);
    }

    _printImage(pixelsArr, output)
    {
        let total = '';
        let prevColor = this.color;
        let prevBackground = this._background;

        for(let row of pixelsArr) {
            let paintedLine = '';
            let block = '  ';

            for(let i = 0; i < row.length; i++) {
                let pixel = row[i];
                let nextPixel = (i < row.length - 1) ? row[i + 1] : null;

                if(nextPixel && pixel.every((value, index) =>
                    value === nextPixel[index]
                )) {
                    block += '  ';
                    continue;
                }
                /* Do not use predefined functions here (it would impact performance) */
                let isTransparent = (pixel.length >= 3) ? _getIsTransparent(pixel[3]) : false;
                this.color = (isTransparent)
                    ? Color.DEFAULT
                    : `38;2;${pixel[0]};${pixel[1]};${pixel[2]}`;
                this._background = (isTransparent)
                    ? Color.DEFAULT
                    : `48;2;${pixel[0]};${pixel[1]};${pixel[2]}`;
                paintedLine += `${TERM_ESC}0;${this.color};${this._background}m${block}`;
                block = '  ';
            }
            paintedLine += TERM_ESC + TERM_RESET;

            switch(output) {
                case 'stderr':
                    printerr(paintedLine);
                    break;
                case 'return':
                    total += paintedLine + '\n';
                    break;
                default:
                    print(paintedLine);
                    break;
            }
        }

        this.color = prevColor;
        this._background = prevBackground;

        return total;
    }
}
