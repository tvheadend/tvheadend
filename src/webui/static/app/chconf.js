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

    function get_lowest_number(nums) {
        nums.sort(function(a, b) { return (a - b); });
        var max = nums[nums.length - 1];
        var low = max + 1;
        for (var i = 1; i <= max; ++i)
            if (nums.indexOf(i) < 0)
                return i;
        return low;
    }

    function assign_low_number(ctx, e, store, sm) {
        var nums = [];
        store.each(function(rec) {
            var number = rec.get('number');
            if (typeof number === 'number' && number > 0)
                nums.push(number);
        });
        if (nums.length === 0)
            nums.push(0);

        if (sm.getCount() === 1) {
            sm.getSelected().set('number', get_lowest_number(nums));
            sm.selectNext();
        } else {
            var sel = sm.getSelections();
            var low = sel[0].get('number');
            low = low ? low : get_lowest_number(nums);
            Ext.each(sel, function(s) {
                while (nums.indexOf(low) >= 0) low++;
                s.set('number', low++);
            });
        }
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
            var m = new Ext.menu.Menu()
            m.add({
                name: 'mapsel',
                tooltip: _('Map selected services to channels'),
                iconCls: 'clone',
                text: _('Map selected services'),
            });
            m.add({
                name: 'mapall',
                tooltip: _('Map all services to channels'),
                iconCls: 'clone',
                text: _('Map all services'),
            });
            return new Ext.Toolbar.Button({
                tooltip: _('Map services to channels'),
                iconCls: 'clone',
                text: _('Map services'),
                menu: m,
                disabled: false
            });
        },
        callback: {
            mapall: tvheadend.service_mapper_all,
            mapsel: tvheadend.service_mapper_none,
        }
    };

    var chopsButton = {
        name: 'chops',
        builder: function() {
            var m = new Ext.menu.Menu()
            m.add({
                name: 'lowno',
                tooltip: _('Assign lowest free channel number'),
                iconCls: 'chnumops_low',
                text: _('Assign Number')
            });
            m.add({
                name: 'noup',
                tooltip: _('Move channel one number up'),
                iconCls: 'chnumops_up',
                text: _('Number Up')
            });
            m.add({
                name: 'nodown',
                tooltip: _('Move channel one number down'),
                iconCls: 'chnumops_down',
                text: _('Number Down')
            });
            m.add({
                name: 'swap',
                tooltip: _('Swap the numbers for the two selected channels'),
                iconCls: 'chnumops_swap',
                text: _('Swap Numbers')
            });
            return new Ext.Toolbar.Button({
                tooltip: _('Channel number operations'),
                iconCls: 'chnumops',
                text: _('Number operations'),
                menu: m,
                disabled: false
            });
        },
        callback: {
            lowno: assign_low_number,
            noup: move_number_up,
            nodown: move_number_down,
            swap: swap_numbers
        }
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
        id: 'channels',
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
        tbar: [mapButton, chopsButton, iconResetButton],
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
