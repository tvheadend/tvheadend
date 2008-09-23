
tvheadend.acleditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
	header: "Enabled",
	dataIndex: 'enabled',
	width: 60
    });

    var streamingColumn = new Ext.grid.CheckColumn({
       header: "Streaming",
       dataIndex: 'streaming',
       width: 100
    });

    var pvrColumn = new Ext.grid.CheckColumn({
       header: "Video Recorder",
       dataIndex: 'pvr',
       width: 100
    });

    var webuiColumn = new Ext.grid.CheckColumn({
       header: "Web Interface",
       dataIndex: 'webui',
       width: 100
    });

    var adminColumn = new Ext.grid.CheckColumn({
       header: "Admin",
       dataIndex: 'admin',
       width: 100
    });

    var cm = new Ext.grid.ColumnModel([
	enabledColumn,
	{
	    header: "Username",
	    dataIndex: 'username',
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Password",
	    dataIndex: 'password',
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Prefix",
	    dataIndex: 'prefix',
	    editor: new fm.TextField({allowBlank: false})
	},
	streamingColumn,
	pvrColumn,
	webuiColumn,
	adminColumn,
	{
	    header: "Comment",
	    dataIndex: 'comment',
	    width: 400,
	    editor: new fm.TextField({})
	}
    ]);
    
    var UserRecord = Ext.data.Record.create([
	'enabled','streaming','pvr','admin','webui','username',
	'prefix','password','comment'
    ]);

    return new tvheadend.tableEditor('Access control', 'accesscontrol', cm,
				     UserRecord,
				     [enabledColumn, streamingColumn,
				      pvrColumn, webuiColumn,
				      adminColumn]);
}
