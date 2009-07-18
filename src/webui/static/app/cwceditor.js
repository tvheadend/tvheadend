
tvheadend.cwceditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 60
    });

    var cm = new Ext.grid.ColumnModel([
	enabledColumn,
	{
	    header: "Hostname",
	    dataIndex: 'hostname',
	    width: 200,
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Port",
	    dataIndex: 'port',
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Username",
	    dataIndex: 'username',
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Password",
	    dataIndex: 'password',
	    renderer: function(value, metadata, record, row, col, store) {
		return '<span class="tvh-grid-unset">Hidden</span>';
	    },
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "DES Key",
	    dataIndex: 'deskey',
	    width: 300,
	    renderer: function(value, metadata, record, row, col, store) {
		return '<span class="tvh-grid-unset">Hidden</span>';
	    },
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Comment",
	    dataIndex: 'comment',
	    width: 400,
	    editor: new fm.TextField()
	}
    ]);

    var rec = Ext.data.Record.create([
	'enabled','hostname','port','username','password','deskey','comment'
    ]);

    return new tvheadend.tableEditor('Code Word Client', 'cwc', cm, rec,
				     [enabledColumn], null,
				     'config_cwc.html', 'key');
}
