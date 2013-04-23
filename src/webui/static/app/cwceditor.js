tvheadend.cwceditor = function() {
	var fm = Ext.form;

	function setMetaAttr(meta, record) {
		var enabled = record.get('enabled');
		if (!enabled) return;

		var connected = record.get('connected');
		if (connected == 1) {
			meta.attr = 'style="color:green;"';
		}
		else {
			meta.attr = 'style="color:red;"';
		}
	}

	var cm = new Ext.grid.ColumnModel({
    defaultSortable: true,
          columns : [ {
            xtype: 'checkcolumn',
	    header : "Enabled",
	    dataIndex : 'enabled',
	    width : 60
          }, {
		header : "Hostname",
		dataIndex : 'hostname',
		width : 200,
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
		header : "Port",
		dataIndex : 'port',
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
		header : "Username",
		dataIndex : 'username',
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
		header : "Password",
		dataIndex : 'password',
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return '<span class="tvh-grid-unset">Hidden</span>';
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
		header : "DES Key",
		dataIndex : 'deskey',
		width : 300,
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return '<span class="tvh-grid-unset">Hidden</span>';
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
          xtype: 'checkcolumn',
		header : "Update Card",
		dataIndex : 'emm',
		width : 100
        }, {
          xtype: 'checkcolumn',
		header : "Update One",
		dataIndex : 'emmex',
		width : 100
        }, {
		header : "Comment",
		dataIndex : 'comment',
		width : 400,
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField()
	} ]});

	var rec = Ext.data.Record.create([ 'enabled', 'connected', 'hostname',
		'port', 'username', 'password', 'deskey', 'emm', 'emmex', 'comment' ]);

	var store = new Ext.data.JsonStore({
		root : 'entries',
		fields : rec,
		url : "tablemgr",
		autoLoad : true,
		id : 'id',
		baseParams : {
			table : 'cwc',
			op : "get"
		}
	});

	var grid = new tvheadend.tableEditor('Code Word Client', 'cwc', cm, rec, [],
                                             store, 'config_cwc.html', 'key');

	tvheadend.comet.on('cwcStatus', function(msg) {
		var rec = store.getById(msg.id);
		if (rec) {
			rec.set('connected', msg.connected);
			grid.getView().refresh();
		}
	});

	return grid;
}
