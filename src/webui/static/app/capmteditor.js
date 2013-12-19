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

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true,
          columns: [ {
            xtype: 'checkcolumn',
		header : "Enabled",
		dataIndex : 'enabled',
		width : 60
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
            xtype: 'checkcolumn',
		header : "OSCam mode",
		dataIndex : 'oscam',
		width : 60

        } , {
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
