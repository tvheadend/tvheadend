function sprintf() {
    var args = arguments, i = 1;
    return args[0].replace(/%((%)|s|d|i|f)/g, function (m) {
        var val = null;
        if (m[2]) {
            val = m[2];
        } else {
            val = args[i];
            switch (m) {
                case '%d':
                case '%i':
                    val = parseInt(val);
                    if (isNaN(val)) val = 0;
                    break;
                case '%f':
                    val = parseFloat(val);
                    if (isNaN(val)) val = 0;
                    break;
            }
            i++;
        }
        return val;
    });
}

function _(s)
{
    var r = tvh_locale[s];
    return typeof r === 'undefined' ? s : r;
}
