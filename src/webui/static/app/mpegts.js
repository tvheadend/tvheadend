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
                    var title = r.data['name'];
                    if (r.data['network']) {
                        if (title) title += ' / ';
                        title += r.data['network'];
                    }
                    return tvheadend.playLink('stream/mux/' + r.id, title);
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

    function header() {
        html += '<table style="font-size:8pt;font-family:monospace;padding:2px"';
        html += '<tr>';
        html += '<th style="width:50px;font-weight:bold">' + _('Index') + '</th>';
        html += '<th style="width:120px;font-weight:bold">' + _('PID') + '</th>';
        html += '<th style="width:100px;font-weight:bold">' + _('Type') + '</th>';
        html += '<th style="width:75px;font-weight:bold">' + _('Language') + '</th>';
        html += '<th style="width:*;font-weight:bold">' + _('Details') + '</th>';
        html += '</tr>';

    }

    function footer() {
        html += '</table>';
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
        if (s.type === 'CA' || s.type === 'CAT') {
            d = _('CAIDS: ');
            if (s.caids === undefined) {
                d += '<em>none</em>';
            } else {
                for (j = 0; j < s.caids.length; j++) {
                    if (j > 0)
                        d += ', ';
                    d += hexstr(s.caids[j].caid) + ':';
                    d += hexstr6(s.caids[j].provider);
                }
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
    footer();

    single('<h3>' + _('After filtering and reordering (without PCR and PMT)') + '</h3>');

    header();
    if (data.fstreams.length)
        for (i = 0; i < data.fstreams.length; i++)
            stream(data.fstreams[i]);
    else
        single(_('None'));
    footer();

    if (data.hbbtv) {
        html += '<h3>' + _('HbbTv') + '</h3>';
        html += '<table style="font-size:8pt;font-family:monospace;padding:2px"';
        html += '<tr>';
        html += '<th style="width:50px;font-weight:bold">' + _('Section') + '</th>';
        html += '<th style="width:50px;font-weight:bold">' + _('Language') + '</th>';
        html += '<th style="width:200px;font-weight:bold">' + _('Name') + '</th>';
        html += '<th style="width:310px;font-weight:bold">' + _('Link') + '</th>';
        html += '</tr>';
        for (var sect in data.hbbtv) {
            for (var appidx in data.hbbtv[sect]) {
                var app = data.hbbtv[sect][appidx];
                if (!app.title) continue;
                for (var titleidx = 0; titleidx < app.title.length; titleidx++) {
                    var title = app.title[titleidx];
                    html += '<tr>';
                    html += '<td>' + sect + '</td>';
                    html += '<td>' + title.lang + '</td>';
                    html += '<td>' + title.name + '</td>';
                    html += '<td><a href="' + app.url + '" target="_blank">' + app.url + '</td>';
                    html += '</tr>';
                }
            }
        }
        html += '</table>';
    }

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

        var unseencb = function(type) {
            tvheadend.Ajax({
                url: 'api/service/removeunseen',
                params: {
                    type: type,
                },    
                success: function(d) {
                    store.reload();
                }
            });
        };

        var maintenanceButton = {
            name: 'misc',
            builder: function() {
                var m = new Ext.menu.Menu()
                m.add({
                    name: 'rmunsnpat',
                    tooltip: _('Remove old services marked as missing in PAT/SDT which were not detected more than 7 days (last seen column)'),
                    iconCls: 'remove',
                    text: _('Remove unseen services (PAT/SDT) (7 days+)'),
                });
                m.add({
                    name: 'rmunsn',
                    tooltip: _('Remove old services which were not detected more than 7 days (last seen column)'),
                    iconCls: 'remove',
                    text: _('Remove all unseen services (7 days+)'),
                });
                return new Ext.Toolbar.Button({
                    tooltip: _('Maintenance operations'),
                    iconCls: 'wrench',
                    text: _('Maintenance'),
                    menu: m,
                    disabled: false
                });
            },
            callback: {
                rmunsnpat: function() { unseencb('pat'); },
                rmunsn: function() { unseencb(''); }
            }
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
        conf.tbar = [mapButton, maintenanceButton];
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
        id: 'services',
        url: 'api/mpegts/service',
        titleS: _('Service'),
        titleP: _('Services'),
        iconCls: 'services',
        tabIndex: index,
        hidemode: true,
        add: false,
        del: true,
        lcol: [
            {
                width: 50,
                header: _('Play'),
                tooltip: _('Play'),
                renderer: function(v, o, r) {
                    var title = r.data['svcname'];
                    if (r.data['provider']) {
                        if (title) title += ' / ';
                        title += r.data['provider'];
                    }
                    return tvheadend.playLink('stream/service/' + r.id, title);
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
        uilevel: 'expert',
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
