
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

    var dvrColumn = new Ext.grid.CheckColumn({
       header: "Video Recorder",
       dataIndex: 'dvr',
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
	    renderer: function(value, metadata, record, row, col, store) {
		return '<i>hidden</i>'
	    },
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Prefix",
	    dataIndex: 'prefix',
	    editor: new fm.TextField({allowBlank: false})
	},
	streamingColumn,
	dvrColumn,
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
	'enabled','streaming','dvr','admin','webui','username',
	'prefix','password','comment'
    ]);

    return new tvheadend.tableEditor('Access control', 'accesscontrol', cm,
				     UserRecord,
				     [enabledColumn, streamingColumn,
				      dvrColumn, webuiColumn,
				      adminColumn],
				     null,
				    'config_access.html');
}
