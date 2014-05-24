tvheadend.cteditor = function() {
    var fm = Ext.form;

    var cm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns: [{
                xtype: 'checkcolumn',
                header: "Enabled",
                dataIndex: 'enabled',
                width: 60
            }, {
                header: "Name",
                dataIndex: 'name',
                editor: new fm.TextField({
                    allowBlank: false
                })
            }, {
                xtype: 'checkcolumn',
                header: "Internal",
                dataIndex: 'internal',
                width: 100
            }, {
                header: "Icon (full URL)",
                dataIndex: 'icon',
                width: 400,
                editor: new fm.TextField({})
            }, {
                xtype: 'checkcolumn',
                header: "Icon has title",
                dataIndex: 'titledIcon',
                width: 100,
                tooltip: 'Set this if the supplied icon has a title embedded. '
                        + 'This will tell displaying application not to superimpose title '
                        + 'on top of logo.'
            }, {
                header: "Comment",
                dataIndex: 'comment',
                width: 400,
                editor: new fm.TextField({})
            }]});

    var ChannelTagRecord = Ext.data.Record.create([
        'enabled', 'name', 'internal', 'icon', 'comment', 'titledIcon']);

    return new tvheadend.tableEditor('Channel Tags', 'channeltags', cm,
            ChannelTagRecord, [],
            null, 'config_tags.html', 'tags');
};
