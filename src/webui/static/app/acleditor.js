tvheadend.acleditor = function() {
    var fm = Ext.form;

    var cm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns: [{
                xtype: 'checkcolumn',
                header: "Enabled",
                dataIndex: 'enabled',
                width: 60
            }, {
                header: "Username",
                dataIndex: 'username',
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                header: "Password",
                dataIndex: 'password',
                renderer: function(value, metadata, record, row, col, store) {
                    return '<span class="tvh-grid-unset">Hidden</span>';
                },
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                header: "Prefix",
                dataIndex: 'prefix',
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                xtype: 'checkcolumn',
                header: "Streaming",
                dataIndex: 'streaming',
                width: 100
            }, {
                xtype: 'checkcolumn',
                header: "Advanced Streaming",
                dataIndex: 'adv_streaming',
                width: 100
            }, {
                xtype: 'checkcolumn',
                header: "Video Recorder",
                dataIndex: 'dvr',
                width: 100
            }, {
                xtype: 'checkcolumn',
                header: "Username Configs (VR)",
                dataIndex: 'dvrallcfg',
                width: 150
            }, {
                xtype: 'checkcolumn',
                header: "Web Interface",
                dataIndex: 'webui',
                width: 100
            }, {
                xtype: 'checkcolumn',
                header: "Admin",
                dataIndex: 'admin',
                width: 100
            }, {
                xtype: 'checkcolumn',
                header: "Username Channel Tag Match",
                dataIndex: 'tag_only',
                width: 150
            }, {
                xtype: 'numbercolumn',
                header: "Min Channel Num",
                dataIndex: 'channel_min',
                width: 120,
                format: '0',
                editor: {
                  xtype: 'numberfield',
                  allowBlank: 'false',
                  minValue: 0,
                  maxValue: 2147483647,
                  allowNegative: false,
                  allowDecimals: false
                }
            }, {
                xtype: 'numbercolumn',
                header: "Max Channel Num",
                dataIndex: 'channel_max',
                width: 120,
                format: '0',
                editor: {
                  xtype: 'numberfield',
                  allowBlank: 'false',
                  minValue: 0,
                  maxValue: 2147483647,
                  allowNegative: false,
                  allowDecimals: false
                }
            }, {
                header: "Channel Tag",
                dataIndex: 'channel_tag',
                width: 200,
                editor: new Ext.form.ComboBox({
                   displayField: 'name',
                   store: tvheadend.channelTags,
                   mode: 'local',
                   editable: true,
                   forceSelection: true,
                   typeAhead: true,
                   triggerAction: 'all',
                   emptyText: 'Match this channel tag...'
                })
            }, {
                header: "Comment",
                dataIndex: 'comment',
                width: 300,
                editor: new fm.TextField({})
            }]});

    var UserRecord = Ext.data.Record.create(
            ['enabled', 'streaming', 'dvr', 'dvrallcfg', 'admin', 'webui', 'username', 'tag_only',
                'prefix', 'password', 'comment', 'channel_min', 'channel_max', 'channel_tag',
                'adv_streaming'
            ]);

    tvheadend.accesscontrolStore = new Ext.data.JsonStore({
        root: 'entries',
        fields: UserRecord,
        url: "tablemgr",
        autoLoad: true,
        id: 'id',
        baseParams: {
            table: "accesscontrol",
            op: "get"
        }
    });

    tvheadend.accesscontrolStore.on('update', function (store, record, operation) {
        if (operation == 'edit') {
            if (record.isModified('channel_tag') && record.data.channel_tag == '(Clear filter)')
                record.set('channel_tag',"");
        }
    });

    return new tvheadend.tableEditor('Access control', 'accesscontrol', cm,
            UserRecord, [], tvheadend.accesscontrolStore, 'config_access.html',
            'group');
};
