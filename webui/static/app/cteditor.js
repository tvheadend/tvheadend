
tvheadend.cteditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
	header: "Enabled",
	dataIndex: 'enabled',
	width: 60
    });

    var internalColumn = new Ext.grid.CheckColumn({
       header: "Internal",
       dataIndex: 'internal',
       width: 100
    });


    var cm = new Ext.grid.ColumnModel([
	enabledColumn,
	{
	    header: "Name",
	    dataIndex: 'name',
	    editor: new fm.TextField({allowBlank: false})
	},
	internalColumn,
	{
	    header: "Comment",
	    dataIndex: 'comment',
	    width: 400,
	    editor: new fm.TextField({})
	}
    ]);
    
    var ChannelTagRecord = Ext.data.Record.create([
	'enabled','name','internal','comment'
    ]);
    
    return new tvheadend.tableEditor('Channel Tags', 'channeltags', cm,
				     ChannelTagRecord,
				     [enabledColumn, internalColumn]);
}
