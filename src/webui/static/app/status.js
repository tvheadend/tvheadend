/**
 *
 */
tvheadend.status_subs = function(panel, index)
{
    var subs = null;
    var store = null;

    function update(m) {
        if (m.reload != null)
            store.reload();

        if (m.updateEntry != null) {
            r = store.getById(m.id);
            if (typeof r === 'undefined') {
                store.reload();
                return;
            }

            r.data.channel = m.channel;
            r.data.service = m.service;
            r.data.state = m.state;
            if (m.descramble) r.data.descramble = m.descramble;
            if (m.profile) r.data.profile = m.profile;
            r.data.errors = m.errors;
            r.data['in'] = m['in'];
            r.data.out = m.out;

            store.afterEdit(r);
            store.fireEvent('updated', store, r, Ext.data.Record.COMMIT);
        }
    }

    function builder() {
        if (subs)
            return;

        store = new Ext.data.JsonStore({
            root: 'entries',
            totalProperty: 'totalCount',
            fields: [
                { name: 'id' },
                { name: 'hostname' },
                { name: 'username' },
                { name: 'title' },
                { name: 'channel' },
                { name: 'service' },
                { name: 'profile' },
                { name: 'state' },
                { name: 'descramble' },
                { name: 'errors' },
                { name: 'in' },
                { name: 'out' },
                {
                    name: 'start',
                    type: 'date',
                    dateFormat: 'U' /* unix time */
                }
            ],
            url: 'api/status/subscriptions',
            autoLoad: true,
            id: 'id'
        });
        tvheadend.subsStore = store;

        tvheadend.comet.on('subscriptions', update);

        function renderBw(value, meta, record) {
            var txt = parseInt(value / 125);
            meta.attr = 'style="cursor:alias;"';
            return '<span class="x-linked">&nbsp;</span>' + txt;
        }

        var subsCm = new Ext.grid.ColumnModel([
            {
                width: 50,
                id: 'id',
                header: _("ID"),
                dataIndex: 'id',
                renderer: function(v) {
                    return ("0000000" + v.toString(16).toUpperCase()).substr(-8);
                }

            },
            {
                width: 50,
                id: 'hostname',
                header: _("Hostname"),
                dataIndex: 'hostname'
            },
            {
                width: 50,
                id: 'username',
                header: _("Username"),
                dataIndex: 'username'
            },
            {
                width: 80,
                id: 'title',
                header: _("Title"),
                dataIndex: 'title'
            },
            {
                width: 50,
                id: 'channel',
                header: _("Channel"),
                dataIndex: 'channel'
            },
            {
                width: 200,
                id: 'service',
                header: _("Service"),
                dataIndex: 'service'
            }, 
            {
                width: 50,
                id: 'profile',
                header: _("Profile"),
                dataIndex: 'profile'
            }, 
            {
                width: 50,
                id: 'start',
                header: _("Start"),
                dataIndex: 'start',
                renderer: function(v) {
                    var dt = new Date(v);
                    return dt.format('D j M H:i');
                }
            },
            {
                width: 50,
                id: 'state',
                header: _("State"),
                dataIndex: 'state'
            },
            {
                width: 80,
                id: 'descramble',
                header: _("Descramble"),
                dataIndex: 'descramble'
            },
            {
                width: 50,
                id: 'errors',
                header: _("Errors"),
                dataIndex: 'errors'
            },
            {
                width: 50,
                id: 'in',
                header: _("Input (kb/s)"),
                dataIndex: 'in',
                listeners: { click: { fn: clicked } },
                renderer: renderBw
            },
            {
                width: 50,
                id: 'out',
                header: _("Output (kb/s)"),
                dataIndex: 'out',
                listeners: { click: { fn: clicked } },
                renderer: renderBw
            }
        ]);
        
        function clicked(column, grid, index, e) {
            if (column.dataIndex == 'in' || column.dataIndex == 'out') {
                var id = grid.getStore().getAt(index).id;
                tvheadend.subscription_bw_monitor(id);
                return false;
            }
        }

        subs = new Ext.grid.GridPanel({
            border: false,
            loadMask: true,
            stripeRows: true,
            disableSelection: true,
            store: store,
            cm: subsCm,
            flex: 1,
            viewConfig: {
                forceFit: true
            }
        });
        
        dpanel.add(subs);
        dpanel.doLayout(false, true);
    }

    function destroyer() {
        if (subs === null || !tvheadend.dynamic)
            return;
        tvheadend.comet.un('subscriptions', update);
        dpanel.removeAll()
        tvheadend.subsStore = null;
        store.destroy();
        store = null;
        subs = null;
    }

    var dpanel = new Ext.Panel({
        border: false,
        header: false,
        layout: 'fit',
        title: _('Subscriptions'),
        iconCls: 'subscriptions'
    });

    tvheadend.paneladd(panel, dpanel, index);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
};


/**
 * Streams
 */
tvheadend.status_streams = function(panel, index)
{
    var grid = null;
    var store = null;

    function update(m) {
        if (m.reload != null) {
            store.reload();
            return;
        }
        if (m.update == null)
           return;
        var r = store.getById(m.uuid);
        if (!r) {
            store.reload();
            return;
        }
        r.data.subs = m.subs;
        r.data.weight = m.weight;
        r.data.signal = m.signal;
        r.data.ber = m.ber;
        r.data.unc = m.unc;
        r.data.snr = m.snr;
        r.data.bps = m.bps;
        r.data.cc = m.cc;
        r.data.te = m.te;
        r.data.signal_scale = m.signal_scale;
        r.data.snr_scale = m.snr_scale;
        r.data.ec_bit = m.ec_bit;
        r.data.tc_bit = m.tc_bit;
        r.data.ec_block = m.ec_block;
        r.data.tc_block = m.tc_block;

        store.afterEdit(r);
        store.fireEvent('updated', store, Ext.data.Record.COMMIT);
    }

    function builder() {
        if (grid)
            return;

        var actions = new Ext.ux.grid.RowActions({
            header: '',
            width: 10,
            actions: [
                {
                    iconCls: 'clean',
                    qtip: _('Clear statistics'),
                    cb: function(grid, rec, act, row) {
                        var uuid = grid.getStore().getAt(row).data.uuid;
                        Ext.MessageBox.confirm(_('Clear statistics'),
                            _('Clear statistics for selected input?'),
                            function(button) {
                                if (button === 'no')
                                    return;
                                Ext.Ajax.request({
                                    url: 'api/status/inputclrstats',
                                    params: { uuid: uuid }
                                });
                            }
                       );
                   }
               }
            ],
            destroy: function() {
            }
        });

        store = new Ext.data.JsonStore({
            root: 'entries',
            totalProperty: 'totalCount',
            fields: [
                { name: 'uuid' },
                { name: 'input' },
                { name: 'username' },
                { name: 'stream' },
                { name: 'subs' },
                { name: 'weight' },
                { name: 'signal' },
                { name: 'ber' },
                { name: 'unc' },
                { name: 'snr' },
                { name: 'bps' },
                { name: 'cc' },
                { name: 'te' },
                { name: 'signal_scale' },
                { name: 'snr_scale' },
                { name: 'ec_bit' },
                { name: 'tc_bit' },
                { name: 'ec_block' },
                { name: 'tc_block' }
            ],
            url: 'api/status/inputs',
            autoLoad: true,
            id: 'uuid'
        });
        tvheadend.streamStatusStore = store;

        tvheadend.comet.on('input_status', update);

        function renderBw(value, meta, record) {
            var txt = parseInt(value / 1024);
            meta.attr = 'style="cursor:alias;"';
            return '<span class="x-linked">&nbsp;</span>' + txt;
        }

        function renderBer(value, item, store) {
            if (store.data.tc_bit == 0)
              return value; // fallback (driver/vendor dependent ber)

            // ber = error_bit_count / total_bit_count
            var ber = store.data.ec_bit / store.data.tc_bit;
            return ber;
        }

        function renderPer(value, item, store) {
            if (value == 0) // value: total_block_count
              return '<span class="tvh-grid-unset">Unknown</span>';

            // per = error_block_count / total_block_count
            var per = store.data.ec_block / value;
            return per;
        }

        var cm = new Ext.grid.ColumnModel([
            actions,
            {
                width: 120,
                header: _("Input"),
                dataIndex: 'input'
            },
            {
                width: 100,
                header: _("Stream"),
                dataIndex: 'stream'
            },
            {
                width: 50,
                header: _("Subs No."),
                dataIndex: 'subs'
            },
            {
                width: 50,
                header: _("Weight"),
                dataIndex: 'weight'
            },
            {
                width: 50,
                header: _("Bandwidth (kb/s)"),
                dataIndex: 'bps',
                renderer: renderBw,
                listeners: { click: { fn: clicked } }
            },
            {
                width: 50,
                header: _("BER"),
                dataIndex: 'ber',
                renderer: renderBer
            },
            {
                width: 50,
                header: _("PER"),
                dataIndex: 'tc_block',
                renderer: renderPer
            },
            {
                width: 50,
                header: _("Uncorrected Blocks"),
                dataIndex: 'unc'
            },
            {
                width: 50,
                header: _("Transport Errors"),
                dataIndex: 'te'
            },
            {
                width: 50,
                header: _("Continuity Errors"),
                dataIndex: 'cc'
            }
        ]);

        function clicked(column, grid, index, e) {
            if (column.dataIndex == 'bps') {
                var id = grid.getStore().getAt(index).id;
                tvheadend.stream_bw_monitor(id);
                return false;
            }
        }

        cm.config.push(new Ext.ux.grid.ProgressColumn({
            header: _("SNR"),
            dataIndex: 'snr',
            width: 85,
            colored: true,
            ceiling: 65535,
            tvh_renderer: function(v, p, record) {
                var scale = record.get('snr_scale');
                if (scale == 1)
                  return v;
                if (scale == 2 && v > 0) {
                  var snr = v * 0.001;
                  return snr.toFixed(1) + " dB";
                }
                return '<span class="tvh-grid-unset">' + _('Unknown') + '</span>';
            },
            destroy: function() {
            }
        }));

        cm.config.push(new Ext.ux.grid.ProgressColumn({
            header: _("Signal Strength"),
            dataIndex: 'signal',
            width: 85,
            colored: true,
            ceiling: 65535,
            tvh_renderer: function(v, p, record) {
                var scale = record.get('signal_scale');
                if (scale == 1)
                  return v;
                if (scale == 2) {
                    var snr = v * 0.001;
                    return snr.toFixed(1) + " dBm";
                }
                return '<span class="tvh-grid-unset">' + _('Unknown') + '</span>';
            },
            destroy: function() {
            }
        }));

        grid = new Ext.grid.GridPanel({
            border: false,
            loadMask: true,
            stripeRows: true,
            disableSelection: true,
            store: store,
            cm: cm,
            flex: 1,
            viewConfig: {
                forceFit: true
            },
            plugins: [actions]
        });
        
        dpanel.add(grid);
        dpanel.doLayout(false, true);
    }

    function destroyer() {
        if (grid === null || !tvheadend.dynamic)
            return;
        tvheadend.comet.un('input_status', update);
        dpanel.removeAll()
        tvheadend.streamStatusStore = null;
        store.destroy();
        store = null;
        grid = null;
    }

    var dpanel = new Ext.Panel({
        id: 'status_streams',
        border: false,
        header: false,
        layout: 'fit',
        title: _('Stream'),
        iconCls: 'stream'
    });

    tvheadend.paneladd(panel, dpanel, index);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
};

/**
 *
 */
tvheadend.status_conns = function(panel, index) {

    var grid = null;
    var store = null;

    function update(m) {
        if (m.reload != null)
            store.reload();
    }

    function builder() {
        if (grid)
            return;

        var actions = new Ext.ux.grid.RowActions({
            header: '',
            width: 10,
            actions: [
                {
                    iconCls: 'cancel',
                    qtip: _('Cancel this connection'),
                    cb: function(grid, rec, act, row) {
                        var id = grid.getStore().getAt(row).data.id;
                        Ext.MessageBox.confirm(_('Cancel Connection'),
                            _('Cancel the selected connection?'),
                            function(button) {
                                if (button === 'no')
                                    return;
                                Ext.Ajax.request({
                                    url: 'api/connections/cancel',
                                    params: { id: id }
                                });
                            }
                       );
                   }
               }
            ],
            destroy: function() {
            }
        });

        store = new Ext.data.JsonStore({
            root: 'entries',
            totalProperty: 'totalCount',
            fields: [
                { name: 'id' },
                { name: 'type' },
                { name: 'peer' },
                { name: 'user' },
                {
                    name: 'started',
                    type: 'date',
                    dateFormat: 'U' /* unix time */
                }
            ],
            url: 'api/status/connections',
            autoLoad: true,
            id: 'id'
        });

        tvheadend.comet.on('connections', update);

        function renderDate(value) {
            var dt = new Date(value);
            return dt.format('Y-m-d H:i:s');
        }

        var cm = new Ext.grid.ColumnModel([
            actions,
            {
                width: 50,
                id: 'type',
                header: _("Type"),
                dataIndex: 'type'
            }, {
                width: 50,
                id: 'peer',
                header: _("IP Address"),
                dataIndex: 'peer'
            }, {
                width: 50,
                id: 'user',
                header: _("Username"),
                dataIndex: 'user'
            }, {
                width: 50,
                id: 'started',
                header: _("Started"),
                dataIndex: 'started',
                renderer: renderDate
            }]);

        grid = new Ext.grid.GridPanel({
            border: false,
            loadMask: true,
            stripeRows: true,
            disableSelection: true,
            store: store,
            cm: cm,
            flex: 1,
            viewConfig: {
                forceFit: true
            },
            plugins: [actions]
        });
        
        dpanel.add(grid);
        dpanel.doLayout(false, true);
    }

    function destroyer() {
        if (grid === null || !tvheadend.dynamic)
            return;
        tvheadend.comet.un('connections', update);
        dpanel.removeAll()
        store.destroy();
        store = null;
        grid = null;
    }

    var dpanel = new Ext.Panel({
        border: false,
        header: false,
        layout: 'fit',
        title: _('Connections'),
        iconCls: 'connections'
    });

    tvheadend.paneladd(panel, dpanel, index);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
};

tvheadend.status = function() {
    var panel = new Ext.TabPanel({
        title: _('Status'),
        autoScroll: true,
        activeTab: 0,
        iconCls: 'eye',
        items: []
    });
    tvheadend.status_streams(panel);
    tvheadend.status_subs(panel);
    tvheadend.status_conns(panel);
    tvheadend.service_mapper_status(panel);
    return panel;
};


tvheadend.subscription_bw_monitor = function(id) {
    var inputSeries = new TimeSeries();
    var outputSeries = new TimeSeries();
    var chart = new SmoothieChart({
        minValue: 0,
        grid: {
            sharpLines: true,
            fillStyle: 'transparent',
            verticalSections: 0,
            millisPerLine: 0
        },
        labels: {
            disabled: false,
            fillStyle: '#000000',
            fontSize: 12
        }
    });

    chart.addTimeSeries(inputSeries, {
        strokeStyle: 'rgb(0, 255, 0)',
        fillStyle: 'rgba(0, 255, 0, 0.5)',
        lineWidth: 3
    });

    chart.addTimeSeries(outputSeries, {
        strokeStyle: 'rgb(255, 0, 255)',
        fillStyle: 'rgba(255, 0, 255, 0.5)',
        lineWidth: 3
    });

    var inputLbl = new Ext.form.Label();
    var outputLbl = new Ext.form.Label();
    var comprLbl = new Ext.form.Label();

    var win = new Ext.Window({
        title: _('Bandwidth monitor'),
        layout: 'fit',
        resizable: false,
        width: 450 + 30,
        height: 150 + 50,
        constrainHeader: true,
        tbar: [inputLbl, '-', outputLbl, '-', comprLbl],
        items: {
            xtype: 'box',
            autoEl: {
                tag: 'canvas',
                width: 450,
                height: 150
            },
            listeners: {
                render: {
                    scope: this,
                    fn: function(item) {
                        chart.streamTo(item.el.dom, 1000);
                    }
                },
                resize: {
                    scope: this,
                    fn: function(item) {
                        chart.render(item.el.dom, 1000);
                    }
                }
            }
        }
    });

    var task = {
        interval: 1000,
        run: function() {
            var store = tvheadend.subsStore;
            var r = store ? store.getById(id) : null;
            if (!store || typeof r === 'undefined') {
                chart.stop();
                Ext.TaskMgr.stop(task);
                return;
            }

            var input = Math.round(r.data['in'] / 125);
            var output = Math.round(r.data.out / 125);
            var ratio = new Number(r.data['in'] / r.data.out).toPrecision(3);

            win.setTitle(r.data.channel);
            inputLbl.setText(_('In') + ': ' + input + ' kb/s');
            outputLbl.setText(_('Out') + ': ' + output + ' kb/s');
            comprLbl.setText(_('Compression ratio') + ': ' + ratio);

            inputSeries.append(new Date().getTime(), input);
            outputSeries.append(new Date().getTime(), output);
        }
    };

    win.on('close', function() {
        chart.stop();
        Ext.TaskMgr.stop(task);
    });

    win.show();

    Ext.TaskMgr.start(task);
};


tvheadend.stream_bw_monitor = function(id) {
    var inputSeries = new TimeSeries();
    var chart = new SmoothieChart({
        minValue: 0,
        grid: {
            sharpLines: true,
            fillStyle: 'transparent',
            verticalSections: 0,
            millisPerLine: 0
        },
        labels: {
            disabled: false,
            fillStyle: '#000000',
            fontSize: 12
        }
    });

    chart.addTimeSeries(inputSeries, {
        strokeStyle: 'rgb(0, 255, 0)',
        fillStyle: 'rgba(0, 255, 0, 0.5)',
        lineWidth: 3
    });

    var inputLbl = new Ext.form.Label();

    var win = new Ext.Window({
        title: _('Bandwidth monitor'),
        layout: 'fit',
        resizable: false,
        width: 450 + 30,
        height: 150 + 50,
        constrainHeader: true,
        tbar: [inputLbl],
        items: {
            xtype: 'box',
            autoEl: {
                tag: 'canvas',
                width: 450,
                height: 150
            },
            listeners: {
                render: {
                    scope: this,
                    fn: function(item) {
                        chart.streamTo(item.el.dom, 15000);
                    }
                },
                resize: {
                    scope: this,
                    fn: function(item) {
                        chart.render(item.el.dom, 15000);
                    }
                }
            }
        }
    });

    var task = {
        interval: 1000,
        run: function() {
            var store = tvheadend.streamStatusStore;
            var r = store ? store.getById(id) : null;
            if (!store || typeof r === 'undefined') {
                chart.stop();
                Ext.TaskMgr.stop(task);
                return;
            }

            win.setTitle(r.data.input + ' (' + r.data.stream + ')');
            var input = Math.round(r.data.bps / 1024);
            inputLbl.setText(_('Input') + ': ' + input + ' kb/s');
            inputSeries.append(new Date().getTime(), input);
        }
    };

    win.on('close', function() {
        chart.stop();
        Ext.TaskMgr.stop(task);
    });

    win.show();

    Ext.TaskMgr.start(task);
};
