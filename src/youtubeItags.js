const Itags = {
  video: {
    h264: {
      standard: {
        240:  133,
        360:  134,
        480:  135,
        720:  136,
        1080: 137,
      },
      hfr: {
        720:  298,
        1080: 299,
      },
    },
  },
  audio: {
    aac: [140],
    opus: [249, 250, 251],
  },
  combined: {
    360: 18,
    720: 22,
  }
};

function _appendItagArray(arr, opts, formats)
{
    const keys = Object.keys(formats);

    for(let fmt of keys) {
        arr.push(formats[fmt]);

        if(
            fmt >= opts.height
            || Math.floor(fmt * 16 / 9) >= opts.width
        )
            break;
    }

    return arr;
}

function getDashItags(opts)
{
    const allowed = {
        video: [],
        audio: (opts.codec === 'h264')
            ? Itags.audio.aac
            : Itags.audio.opus
    };

    for(let type of opts.types) {
        const formats = Itags.video[opts.codec][type];
        _appendItagArray(allowed.video, opts, formats);
    }

    return allowed;
}

function getCombinedItags(opts)
{
    const arr = [];
    _appendItagArray(arr, opts, Itags.combined);

    return arr;
}
