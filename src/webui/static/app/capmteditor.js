tvheadend.capmteditor = function() {
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
	var selectMode = new Ext.form.ComboBox({
		displayField:'name',
		valueField: 'res',
		value: 2,
		mode: 'local',
		editable: false,
		triggerAction: 'all',
		emptyText: 'Select mode...',
		store: new Ext.data.SimpleStore({
			fields: ['res','name'],
			id: 0,
			data: [
				['2','Recent OSCam (svn rev >= 9063)'],
				['1','Older OSCam'],
				['0','Wrapper (capmt_ca.so)']
			]
		})
	});

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true,
          columns: [ {
            xtype: 'checkcolumn',
		header : "Enabled",
		dataIndex : 'enabled',
		width : 60
	}, {
		header: "Mode",
		dataIndex: 'oscam',
		width: 150,
		editor: selectMode
	}, {
		header : "Camd.socket Filename",
		dataIndex : 'camdfilename',
		width : 200,
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField({
			allowBlank : false
		})
	}, {
		header : "Listenport",
		dataIndex : 'port',
		renderer : function(value, metadata, record, row, col, store) {
			setMetaAttr(metadata, record);
			return value;
		},
		editor : new fm.TextField({
			allowBlank : false
		})
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

	var rec = Ext.data.Record.create([ 'enabled', 'connected', 'camdfilename',
		'port', 'oscam', 'comment' ]);

	store = new Ext.data.JsonStore({
		root : 'entries',
		fields : rec,
		url : "tablemgr",
		autoLoad : true,
		id : 'id',
		baseParams : {
			table : 'capmt',
			op : "get"
		}
	});

	tvheadend.comet.on('capmtStatus', function(server) {
		var rec = store.getById(server.id);
		if (rec) {
			rec.set('connected', server.connected);
		}
	});

	return new tvheadend.tableEditor('Capmt Connections', 'capmt', cm, rec,
		[ ], store, 'config_capmt.html', 'key');
}
