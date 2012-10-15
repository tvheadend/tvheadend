tvheadend.statusTable = function(title, dtable, cm, rec, plugins, store,
	helpContent, icon) {

	var selModel = new Ext.grid.RowSelectionModel({
		singleSelect : false
	});

	function refreshRecords() {
		store.reload();
	};

	var grid = new Ext.grid.EditorGridPanel({
		title : title,
		iconCls : icon,
		plugins : plugins,
		store : store,
		clicksToEdit : 2,
		cm : cm,
		viewConfig : {
			forceFit : true
		},
		selModel : selModel,
		stripeRows : true,
		tbar : [
			{
				tooltip : 'Refresh the display',
				iconCls : 'wand',
				text : 'Refresh display',
				handler : refreshRecords
			} ]
	});
	return grid;
}

tvheadend.storeStatusUserlist = new Ext.data.JsonStore({
	root : 'entries',
	id : 'id',
	url : 'status/userlist',
	baseParams : {
		op : 'streamdata'
	},
	fields : [ 'id', 'username', 'startlog', 'currlog', 'ip', 'type',
			'streamdata' ],
	autoLoad : true
});
tvheadend.storeStatusUserlist.setDefaultSort('username', 'ASC');
tvheadend.comet.on('statususerlist', function(m) {
	if (m.reload != null)
		tvheadend.storeStatusUserlist.reload();
});

tvheadend.statususerlist = function() {
	var fm = Ext.form;

	var cm = new Ext.grid.ColumnModel({
		defaultSortable : true,
		columns : [ {
			header : "ID",
			dataIndex : 'id',
			width : 10,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "Username",
			dataIndex : 'username',
			width : 30,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "Startlog",
			dataIndex : 'startlog',
			width : 40,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "Currlog",
			dataIndex : 'currlog',
			width : 40,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "IP",
			dataIndex : 'ip',
			width : 40,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "Type",
			dataIndex : 'type',
			width : 40,
			editor : new fm.TextField({
				allowBlank : false
			})
		}, {
			header : "Streamdata",
			dataIndex : 'streamdata',
			width : 40,
			editor : new fm.TextField({
				allowBlank : false
			})
		} ]
	});

	var UserStatusRecord = Ext.data.Record.create([ 'id', 'username',
			'startlog', 'currlog', 'ip', 'type', 'streamdata' ]);

	return new tvheadend.statusTable('User Access List', 'statususerlist', cm,
                       UserStatusRecord, null, tvheadend.storeStatusUserlist,
                       'status_userlist.html', 'group');

}
