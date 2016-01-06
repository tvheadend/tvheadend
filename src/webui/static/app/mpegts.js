/*
 * DVB network
 */

tvheadend.networks = function(panel, index)
{
    if (!tvheadend.network_list) {
        tvheadend.network_list = new Ext.data.JsonStore({
            url: 'api/idnode/load',
            baseParams: { 'class': 'mpegts_network', 'enum': 1 },
            root: 'entries',
            fields: ['key', 'val'],
            id: 'key',
            autoLoad: true
        });
        tvheadend.network_builders = new Ext.data.JsonStore({
            url: 'api/mpegts/network/builders',
            root: 'entries',
            fields: ['class', 'caption', 'order', 'groups', 'props'],
            id: 'class',
            autoLoad: true
        });
        tvheadend.comet.on('mpegts_network', function() {
            // TODO: Might be a bit excessive
            tvheadend.network_builders.reload();
            tvheadend.network_list.reload();
        });
    }

    var scanButton = {
        name: 'scan',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Force new scan (all muxes) for selected networks'),
                iconCls: 'find',
                text: _('Force Scan'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/mpegts/network/scan',
                    params: {
                        uuid: Ext.encode(uuids)
                    },    
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };

    function selected(s, abuttons) {
        abuttons.scan.setDisabled(!s || s.length <= 0);
    }

    tvheadend.idnode_grid(panel, {
        id: 'mpegts_network',
        url: 'api/mpegts/network',
        titleS: _('Network'),
        titleP: _('Networks'),
        iconCls: 'networks',
        tabIndex: index,
        help: function() {
            new tvheadend.help(_('Networks'), 'config_networks.html');
        },
        tbar: [scanButton],
        add: {
            titleS: _('Network'),
            select: {
                label: _('Type'),
                store: tvheadend.network_builders,
                fullRecord: true,
                displayField: 'caption',
                valueField: 'class'
            },
            create: {
                url: 'api/mpegts/network/create'
            }
        },
        del: true,
        selected: selected,
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
        titleS: _('Mux'),
        titleP: _('Muxes'),
        iconCls: 'muxes',
        tabIndex: index,
        hidemode: true,
        help: function() {
            new tvheadend.help(_('Muxes'), 'config_muxes.html');
        },            
        add: {
            titleS: _('Mux'),
            select: {
                label: _('Network'),
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
                header: _('Play'),
                tooltip: _('Play'),
                renderer: function(v, o, r) {
                    var title = r.data['name'] + ' / ' + r.data['network'];
                    return "<a href='play/stream/mux/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>" + _("Play") + "</a>";
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
            d = _('CAIDS: ');
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
        single(_('None'));

    single('&nbsp;');
    single('<h3>' + _('After filtering and reordering (without PCR and PMT)') + '</h3>');
    header();

    if (data.fstreams.length)
        for (i = 0; i < data.fstreams.length; i++)
            stream(data.fstreams[i]);
    else
        single('<p>' + _('None') + '</p>');

    var win = new Ext.Window({
        title: _('Service details for') + ' ' + data.name,
        iconCls: 'info',
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
    function builder(conf) {
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
                mapsel: tvheadend.service_mapper_sel,
            }
        };
        
        var selected = function(s, abuttons)
        {
            if (s.getCount() > 0)
                abuttons.map.setText(_('Map Selected'));
            else
                abuttons.map.setText(_('Map All'));
        };

        var actions = new Ext.ux.grid.RowActions({
            header: _('Details'),
            width: 10,
            actions: [{
                    iconCls: 'info',
                    qtip: _('Detailed stream info'),
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
            }],
            destroy: function() {
            }
        });
        conf.tbar = [mapButton];
        conf.selected = selected;
        conf.lcol[1] = actions;
        conf.plugins = [actions];
    }
    function destroyer(conf) {
        delete conf.tbar;
        delete conf.plugins;
        conf.lcol[1] = {};
        conf.selected = null;
    }
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/service',
        titleS: _('Service'),
        titleP: _('Services'),
        iconCls: 'services',
        tabIndex: index,
        hidemode: true,
        add: false,
        del: true,
        help: function() {
            new tvheadend.help(_('Services'), 'config_services.html');
        },         
        lcol: [
            {
                width: 50,
                header: _('Play'),
                tooltip: _('Play'),
                renderer: function(v, o, r) {
                    var title = r.data['svcname'] + ' / ' + r.data['provider'];
                    return "<a href='play/stream/service/" + r.id +
                           "?title=" + encodeURIComponent(title) + "'>" + _('Play') + "</a>";
                }
            },
            {
                /* placeholder for actions */
            }
        ],
        sort: {
            field: 'svcname',
            direction: 'ASC'
        },
        builder: builder,
        destroyer: destroyer
    });
};

tvheadend.mux_sched = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/mpegts/mux_sched',
        titleS: _('Mux Scheduler'),
        titleP: _('Mux Schedulers'),
        iconCls: 'muxSchedulers',
        tabIndex: index,
        help: function() {
            new tvheadend.help(_('Mux Schedulers'), 'config_muxsched.html');
        },          
        hidemode: true,
        add: {
            url: 'api/mpegts/mux_sched',
            titleS: _('Mux Scheduler'),
            create: {
                url: 'api/mpegts/mux_sched/create'
            }
        },
        del: true
    });
};
