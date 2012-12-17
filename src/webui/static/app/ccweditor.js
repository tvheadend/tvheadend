tvheadend.ccweditor = function() {
	var fm = Ext.form;

	var enabledColumn = new Ext.grid.CheckColumn({
		header : "Enabled",
		dataIndex : 'enabled',
		width : 60
	});

	function setMetaAttr(meta, record) {
		var enabled = record.get('enabled');
		if (enabled == 1) {
			meta.attr = 'style="color:green;"';
		}
		else {
			meta.attr = 'style="color:red;"';
		}
	}

	var cm = new Ext.grid.ColumnModel({
            defaultSortable: true,
            columns: [ enabledColumn, {
		header: "CAID",
		dataIndex: 'caid',
		renderer: function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor: new fm.TextField({allowBlank: false})
	}, {
		header: "Mux ID",
		dataIndex: 'tid',
		renderer: function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
		    return value;
		},
		editor: new fm.TextField({allowBlank: false})
	}, {
		header: "SID",
		dataIndex: 'sid',
		renderer: function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor: new fm.TextField({allowBlank: false})
	}, {
		header: "Key",
		dataIndex: 'key',
		width: 200,
		renderer: function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor: new fm.TextField({allowBlank: false})
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

	var rec = Ext.data.Record.create([ 'enabled','caid','tid','sid','key','comment' ]);

	store = new Ext.data.JsonStore({
		root: 'entries',
		fields: rec,
		url: "tablemgr",
		autoLoad: true,
		id: 'id',
		baseParams: {table: 'ccw', op: "get"}
	});

	tvheadend.comet.on('ccwStatus', function(server) {
		var rec = store.getById(server.id);
		if(rec){
			rec.set('connected', server.connected);
		}
	});

	return new tvheadend.tableEditor('CCW Keys', 'ccw', cm, rec,
				     [enabledColumn], store,
				     'config_ccw.html', 'key');
}
