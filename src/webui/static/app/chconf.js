/**
 * Channel tags
 */
insertChannelTagsClearOption = function( scope, records, options ){
    var placeholder = Ext.data.Record.create(['key', 'val']);
    scope.insert(0,new placeholder({key: '-1', val: _('(Clear filter)')}));
};

tvheadend.channelTags = tvheadend.idnode_get_enum({
    url: 'api/channeltag/list',
    event: 'channeltag',
    listeners: {
        'load': insertChannelTagsClearOption
    }
});

tvheadend.comet.on('channeltags', function(m) {
    if (m.reload != null)
        tvheadend.channelTags.reload();
});

/**
 * Channels
 */
tvheadend.channelrec = new Ext.data.Record.create(
        ['name', 'chid', 'epggrabsrc', 'tags', 'ch_icon', 'epg_pre_start',
            'epg_post_end', 'number']);

insertChannelClearOption = function( scope, records, options ){
    var placeholder = Ext.data.Record.create(['key', 'val']);
    scope.insert(0,new placeholder({key: '-1', val: _('(Clear filter)')}));
};

tvheadend.channels = tvheadend.idnode_get_enum({
    url: 'api/channel/list',
    listeners: {
        'load': insertChannelClearOption
    }
});

tvheadend.channel_tab = function(panel, index)
{
    function decode_dot_number(number) {
        var a = number.split('.');
        if (a.length !== 2)
            return null;
        a[0] = parseInt(a[0]);
        a[1] = parseInt(a[1]);
        return a;
    }

    function assign_low_number(ctx, e, store, sm) {
        if (sm.getCount() !== 1)
            return;

        var nums = [];
        store.each(function() {
            var number = this.data.number;
            if (typeof number === "number" && number > 0)
                nums.push(number);
        });

        if (nums.length === 0)
        {
            sm.getSelected().set('number', 1);
            return;
        }

        nums.sort(function(a, b) {
            return (a - b);
        });

        var max = nums[nums.length - 1];
        var low = max + 1;

        for (var i = 1; i <= max; ++i)
        {
            var ct = false;
            for (var j = 0; j < nums.length; ++j)
                if (nums[j] === i)
                {
                    ct = true;
                    break
                }
            if (!ct)
            {
                low = i;
                break;
            }
        }

        sm.getSelected().set('number', low);
        sm.selectNext();
    }

    function move_number_up(ctx, e, store, sm) {
        Ext.each(sm.getSelections(), function(channel) {
            var number = channel.data.number;
            if (typeof number !== "string")
                channel.set('number', number + 1);
            else {
                a = decode_dot_number(number);
                if (a !== null)
                    channel.set('number', a[0] + '.' + (a[1] + 1));
            }
        });
    }

    function move_number_down(ctx, e, store, sm) {
        Ext.each(sm.getSelections(), function(channel) {
            var number = channel.data.number;
            if (typeof number !== "string" && number > 0)
                channel.set('number', number - 1);
            else {
                a = decode_dot_number(number);
                if (a !== null && a[1] > 0)
                  channel.set('number', a[0] + '.' + (a[1] - 1));
            }
        });
    }

    function swap_numbers(ctx, e, store, sm) {
        if (sm.getCount() !== 2)
            return;

        var sel = sm.getSelections();
        var tmp = sel[0].data.number;

        sel[0].set('number', sel[1].data.number);
        sel[1].set('number', tmp);
    }

    function reset_icons(ctx, e, store, sm) {
        Ext.each(sm.getSelections(), function(channel) {
           channel.set('icon', '');
        });
    }

    var mapButton = {
        name: 'map',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Map services to channels'),
                iconCls: 'clone',
                text: _('Map Services'),
                disabled: false
            });
        },
        callback: tvheadend.service_mapper
    };

    var lowNoButton = {
        name: 'lowno',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Assign lowest free channel number'),
                iconCls: 'bullet_add',
                text: _('Assign Number'),
                disabled: false
            });
        },
        callback: assign_low_number
    };

    var noUpButton = {
        name: 'noup',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Move channel one number up'),
                iconCls: 'arrow_up',
                text: _('Number Up'),
                disabled: false
            });
        },
        callback: move_number_up
    };

    var noDownButton = {
        name: 'nodown',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Move channel one number down'),
                iconCls: 'arrow_down',
                text: _('Number Down'),
                disabled: false
            });
        },
        callback: move_number_down
    };

    var noSwapButton = {
        name: 'swap',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Swap the numbers for the two selected channels'),
                iconCls: 'arrow_switch',
                text: _('Swap Numbers'),
                disabled: false
            });
        },
        callback: swap_numbers
    };

    var iconResetButton = {
        name: 'iconreset',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Reset (clear) the selected icon URLs'),
                iconCls: 'resetIcon',
                text: _('Reset Icon'),
                disabled: false
            });
        },
        callback: reset_icons
    };

    tvheadend.idnode_grid(panel, {
        url: 'api/channel',
        all: 1,
        comet: 'channel',
        titleS: _('Channel'),
        titleP: _('Channels'),
        iconCls: 'channels',
        tabIndex: index,
        help: function() {
            new tvheadend.help(_('Channels'), 'config_channels.html');
        },           
        add: {
            url: 'api/channel',
            create: {}
        },
        del: true,
        tbar: [mapButton, lowNoButton, noUpButton, noDownButton, noSwapButton, iconResetButton],
        lcol: [
            {
                width: 50,
                header: _('Play'),
                tooltip: _('Play'),
                renderer: function(v, o, r) {
                    var title = '';
                    if (r.data['number'])
                      title += r.data['number'] + ' : ';
                    title += r.data['name'];
                    return "<a href='play/stream/channel/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>" + _('Play') + "</a>";
                }
            }
        ],
        sort: {
            field: 'number',
            direction: 'ASC'
        }
    });
};
