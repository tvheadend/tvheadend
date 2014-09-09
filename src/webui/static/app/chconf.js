/**
 * Channel tags
 */
insertChannelTagsClearOption = function( scope, records, options ){
    var placeholder = Ext.data.Record.create(['key', 'val']);
    scope.insert(0,new placeholder({key: '-1', val: '(Clear filter)'}));
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
    scope.insert(0,new placeholder({key: '-1', val: '(Clear filter)'}));
};

tvheadend.channels = new Ext.data.JsonStore({
    url: 'api/channel/list',
    root: 'entries',
    fields: ['key', 'val'],
    id: 'key',
    autoLoad: true,
    sortInfo: {
        field: 'val',
        direction: 'ASC'
    },
    listeners: {
        'load': insertChannelClearOption
    }
});

tvheadend.comet.on('channels', function(m) {
    if (m.reload != null)
        tvheadend.channels.reload();
});

tvheadend.channel_tab = function(panel, index)
{
    function assign_low_number(ctx, e, store, sm) {
        if (sm.getCount() !== 1)
            return;

        var nums = [];
        store.each(function() {
            if (this.data.number > 0)
                nums.push(this.data.number);
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
            channel.set('number', number + 1);
        });
    }

    function move_number_down(ctx, e, store, sm) {
        Ext.each(sm.getSelections(), function(channel) {
            var number = channel.data.number;
            
            // Don't decrement past zero
            if (number > 0)
                channel.set('number', number - 1);
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

    var mapButton = {
        name: 'map',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Map services to channels',
                iconCls: 'clone',
                text: 'Map Services',
                disabled: false
            });
        },
        callback: tvheadend.service_mapper
    };

    var lowNoButton = {
        name: 'lowno',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Assign lowest free channel number',
                iconCls: 'bullet_add',
                text: 'Assign Number',
                disabled: false
            });
        },
        callback: assign_low_number
    };

    var noUpButton = {
        name: 'noup',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Move channel one number up',
                iconCls: 'arrow_up',
                text: 'Number Up',
                disabled: false
            });
        },
        callback: move_number_up
    };

    var noDownButton = {
        name: 'nodown',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Move channel one number down',
                iconCls: 'arrow_down',
                text: 'Number Down',
                disabled: false
            });
        },
        callback: move_number_down
    };

    var noSwapButton = {
        name: 'swap',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Swap the two selected channels numbers',
                iconCls: 'arrow_switch',
                text: 'Swap Numbers',
                disabled: false
            });
        },
        callback: swap_numbers
    };

    tvheadend.idnode_grid(panel, {
        url: 'api/channel',
        comet: 'channel',
        titleS: 'Channel',
        titleP: 'Channels',
        tabIndex: index,
        help: function() {
            new tvheadend.help('Channels', 'config_channels.html');
        },           
        add: {
            url: 'api/channel',
            create: {}
        },
        del: true,
        tbar: [mapButton, lowNoButton, noUpButton, noDownButton, noSwapButton],
        lcol: [
            {
                width: 50,
                header: 'Play',
                renderer: function(v, o, r) {
                    var title = '';
                    if (r.data['number'])
                      title += r.data['number'] + ' : ';
                    title += r.data['name'];
                    return "<a href='play/stream/channel/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>Play</a>";
                }
            }
        ],
        sort: {
            field: 'number',
            direction: 'ASC'
        }
    });
};
