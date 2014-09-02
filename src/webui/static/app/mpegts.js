/*
 * DVB network
 */

tvheadend.network_builders = new Ext.data.JsonStore({
    url: 'api/mpegts/network/builders',
    root: 'entries',
    fields: ['class', 'caption', 'props'],
    id: 'class',
    autoLoad: true
});

tvheadend.network_list = new Ext.data.JsonStore({
    url: 'api/idnode/load',
    baseParams: {class: 'mpegts_network', enum: 1},
    root: 'entries',
    fields: ['key', 'val'],
    id: 'key',
    autoLoad: true
});

tvheadend.comet.on('mpegts_network', function() {
    // TODO: Might be a bit excessive
    tvheadend.network_builders.reload();
    tvheadend.network_list.reload();
});

tvheadend.networks = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/network',
        titleS: 'Network',
        titleP: 'Networks',
        tabIndex: index,
        help: function() {
            new tvheadend.help('Networks', 'config_networks.html');
        },            
        add: {
            titleS: 'Network',
            select: {
                label: 'Type',
                store: tvheadend.network_builders,
                displayField: 'caption',
                valueField: 'class',
                propField: 'props'
            },
            create: {
                url: 'api/mpegts/network/create'
            }
        },
        del: true,
        sort: {
            field: 'networkname',
            direction: 'ASC'
        }
    });
};

tvheadend.muxes = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/mux',
        titleS: 'Mux',
        titleP: 'Muxes',
        tabIndex: index,
        hidemode: true,
        help: function() {
            new tvheadend.help('Muxes', 'config_muxes.html');
        },            
        add: {
            titleS: 'Mux',
            select: {
                label: 'Network',
                store: tvheadend.network_list,
                valueField: 'key',
                displayField: 'val',
                clazz: {
                    url: 'api/mpegts/network/mux_class'
                }
            },
            create: {
                url: 'api/mpegts/network/mux_create'
            }
        },
        del: true,
        lcol: [
            {
                width: 50,
                header: 'Play',
                renderer: function(v, o, r) {
                    var title = r.data['name'] + ' / ' + r.data['network'];
                    return "<a href='play/stream/mux/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>Play</a>";
                }
            }
        ],
        sort: {
            field: 'name',
            direction: 'ASC'
        }
    });
};

tvheadend.show_service_streams = function(data) {
    var i, j;
    var html = '';

    function hexstr(d) {
        return ('0000' + d.toString(16)).slice(-4);
    }

    function hexstr6(d) {
        return ('000000' + d.toString(16)).slice(-6);
    }

    function fixstr(d) {
        var r = d.toString();
        var l = r.length;
        var i;
        for (i = l; i < 5; i++) {
            r = '&nbsp;' + r;
        }
        return r;
    }

    function header( ) {
        html += '<table style="font-size:8pt;font-family:monospace;padding:2px"';
        html += '<tr>';
        html += '<th style="width:50px;font-weight:bold">Index</th>';
        html += '<th style="width:120px;font-weight:bold">PID</th>';
        html += '<th style="width:100px;font-weight:bold">Type</th>';
        html += '<th style="width:75px;font-weight:bold">Language</th>';
        html += '<th style="width:*;font-weight:bold">Details</th>';
        html += '</tr>';

    }

    function single(s) {
        html += '<tr><td colspan="5">' + s + '</td></tr>';
    }

    function stream(s) {
        var d = '&nbsp;';
        var p = '0x' + hexstr(s.pid) + '&nbsp;/&nbsp;' + fixstr(s.pid);

        html += '<tr>';
        html += '<td>' + (s.index > 0 ? s.index : '&nbsp;') + '</td>';
        html += '<td>' + p + '</td>';
        html += '<td>' + s.type + '</td>';
        html += '<td>' + (s.language || '&nbsp;') + '</td>';
        if (s.type === 'CA') {
            d = 'CAIDS: ';
            for (j = 0; j < s.caids.length; j++) {
                if (j > 0)
                    d += ', ';
                d += hexstr(s.caids[j].caid) + ':';
                d += hexstr6(s.caids[j].provider);
            }
        }
        html += '<td>' + d + '</td>';
        html += '</tr>';
    }

    header();

    if (data.streams.length) {
        for (i = 0; i < data.streams.length; i++)
            stream(data.streams[i]);
    } else
        single('None');

    single('&nbsp;');
    single('<h3>After filtering and reordering (without PCR and PMT)</h3>');
    header();

    if (data.fstreams.length)
        for (i = 0; i < data.fstreams.length; i++)
            stream(data.fstreams[i]);
    else
        single('<p>None</p>');

    var win = new Ext.Window({
        title: 'Service details for ' + data.name,
        layout: 'fit',
        width: 650,
        height: 400,
        plain: true,
        bodyStyle: 'padding: 5px',
        html: html,
        autoScroll: true,
        autoShow: true
    });
    win.show();
};

tvheadend.services = function(panel, index)
{
    var mapButton = new Ext.Toolbar.Button({
        tooltip: 'Map services to channels',
        iconCls: 'clone',
        text: 'Map All',
        callback: tvheadend.service_mapper,
        disabled: false
    });
    var selected = function(s)
    {
        if (s.getCount() > 0)
            mapButton.setText('Map Selected');
        else
            mapButton.setText('Map All');
    };
    var actions = new Ext.ux.grid.RowActions({
        header: 'Details',
        width: 10,
        actions: [{
                iconCls: 'info',
                qtip: 'Detailed stream info',
                cb: function(grid, rec, act, row, col) {
                    Ext.Ajax.request({
                        url: 'api/service/streams',
                        params: {
                            uuid: rec.id
                        },
                        success: function(r, o) {
                            var d = Ext.util.JSON.decode(r.responseText);
                            tvheadend.show_service_streams(d);
                        }
                    });
                }
            }]
    });
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/service',
        titleS: 'Service',
        titleP: 'Services',
        tabIndex: index,
        hidemode: true,
        add: false,
        del: false,
        selected: selected,
        tbar: [mapButton],
        help: function() {
            new tvheadend.help('Services', 'config_services.html');
        },         
        lcol: [
            {
                width: 50,
                header: 'Play',
                renderer: function(v, o, r) {
                    var title = r.data['svcname'] + ' / ' + r.data['provider'];
                    return "<a href='play/stream/service/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>Play</a>";
                }
            },
            actions
        ],
        plugins: [actions],
        sort: {
            field: 'svcname',
            direction: 'ASC'
        }
    });
};

tvheadend.mux_sched = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/mux_sched',
        titleS: 'Mux Scheduler',
        titleP: 'Mux Schedulers',
        tabIndex: index,
        help: function() {
            new tvheadend.help('Mux Schedulers', 'config_muxsched.html');
        },          
        hidemode: true,
        add: {
            url: 'api/mpegts/mux_sched',
            titleS: 'Mux Scheduler',
            create: {
                url: 'api/mpegts/mux_sched/create'
            }
        },
        del: true
    });
};
