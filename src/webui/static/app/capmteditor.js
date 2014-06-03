tvheadend.capmteditor = function() {
    var fm = Ext.form;

    function setMetaAttr(meta, record) {
        var enabled = record.get('enabled');
        if (!enabled)
            return;

        var connected = record.get('connected');
        if (connected === 2) {
            meta.attr = 'style="color:green;"';
        }
        else if (connected === 1) {
            meta.attr = 'style="color:orange;"';
        }
        else {
            meta.attr = 'style="color:red;"';
        }
    }
    var selectMode = new Ext.form.ComboBox({
        displayField: 'name',
        valueField: 'res',
        value: 2,
        mode: 'local',
        editable: false,
        triggerAction: 'all',
        emptyText: 'Select mode...',
        store: new Ext.data.SimpleStore({
            fields: ['res', 'name'],
            id: 0,
            data: [
                ['4', 'Patched OSCam (unix socket)'],
                ['3', 'Recent OSCam (svn rev >= 9574 - TCP)'],
                ['2', 'Recent OSCam (svn rev >= 9095)'],
                ['1', 'Older OSCam'],
                ['0', 'Wrapper (capmt_ca.so)']
            ]
        })
    });

    var cm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns: [{
                xtype: 'checkcolumn',
                header: "Enabled",
                dataIndex: 'enabled',
                width: 60
            }, {
                header: "Mode",
                dataIndex: 'oscam',
                width: 150,
                editor: selectMode
            }, {
                header: "Camd.socket Filename / IP Address (mode 3)",
                dataIndex: 'camdfilename',
                width: 200,
                renderer: function(value, metadata, record, row, col, store) {
                    setMetaAttr(metadata, record);
                    return value;
                },
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                header: "Listen/Connect Port",
                dataIndex: 'port',
                renderer: function(value, metadata, record, row, col, store) {
                    setMetaAttr(metadata, record);
                    return value;
                },
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                header: "Comment",
                dataIndex: 'comment',
                width: 400,
                renderer: function(value, metadata, record, row, col, store) {
                    setMetaAttr(metadata, record);
                    return value;
                },
                editor: new fm.TextField()
            }]});

    var rec = Ext.data.Record.create(['enabled', 'connected', 'camdfilename',
        'port', 'oscam', 'comment']);

    store = new Ext.data.JsonStore({
        root: 'entries',
        fields: rec,
        url: "tablemgr",
        autoLoad: true,
        id: 'id',
        baseParams: {
            table: 'capmt',
            op: "get"
        }
    });

    tvheadend.comet.on('capmt', function(server) {
        var rec = store.getById(server.id);
        if (rec) {
            rec.set('connected', server.connected);
        }
    });

    return new tvheadend.tableEditor('Capmt Connections', 'capmt', cm, rec,
            [], store, 'config_capmt.html', 'key');
};
