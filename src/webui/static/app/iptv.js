/**
 * IPTV service grid
 */
tvheadend.iptv = function(adapterId) {

  var servicetypeStore = new Ext.data.JsonStore({
	  root : 'entries',
	  id : 'val',
	  url : '/iptv/services',
	  baseParams : {
		  op : 'servicetypeList'
	  },
	  fields : [ 'val', 'str' ],
	  autoLoad : false
  });

	var fm = Ext.form;

	var enabledColumn = new Ext.grid.CheckColumn({
		header : "Enabled",
		dataIndex : 'enabled',
		width : 45
	});

	var actions = new Ext.ux.grid.RowActions({
		header : '',
		dataIndex : 'actions',
		width : 45,
		actions : [ {
			iconCls : 'info',
			qtip : 'Detailed information about service',
			cb : function(grid, record, action, row, col) {
				Ext.Ajax.request({
					url : "servicedetails/" + record.id,
					success : function(response, options) {
						r = Ext.util.JSON.decode(response.responseText);
						tvheadend.showTransportDetails(r);
					}
				})
			}
		} ]
	});

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true,
  columns : [
		enabledColumn,
		{
			header : "Channel name",
			dataIndex : 'channelname',
			width : 150,
			renderer : function(value, metadata, record, row, col, store) {
				return value ? value
					: '<span class="tvh-grid-unset">Unmapped</span>';
			},
			editor : new fm.ComboBox({
				store : tvheadend.channels,
				allowBlank : true,
				typeAhead : true,
				minChars : 2,
				lazyRender : true,
				triggerAction : 'all',
				mode : 'local',
				displayField : 'name'
			})
		},
		{
			header : "Interface",
			dataIndex : 'interface',
			width : 100,
			renderer : function(value, metadata, record, row, col, store) {
				return value ? value : '<span class="tvh-grid-unset">Unset</span>';
			},
			editor : new fm.TextField({
				allowBlank : false
			})
		},
		{
			header : "Group",
			dataIndex : 'group',
			width : 100,
			renderer : function(value, metadata, record, row, col, store) {
				return value ? value : '<span class="tvh-grid-unset">Unset</span>';
			},
			editor : new fm.TextField({
				allowBlank : false
			})
		},
		{
			header : "UDP Port",
			dataIndex : 'port',
			width : 60,
			editor : new fm.NumberField({
				minValue : 1,
				maxValue : 65535
			})
		},
		{
			header : "Service ID",
			dataIndex : 'sid',
			width : 50,
			hidden : true
		},
		{
			header : 'Service Type',
			width : 100,
			dataIndex : 'stype',
			hidden : true,
			editor : new fm.ComboBox({
				valueField : 'val',
				displayField : 'str',
				forceSelection : false,
				editable : false,
				mode : 'local',
				triggerAction : 'all',
				store : servicetypeStore
			}),
			renderer : function(value, metadata, record, row, col, store) {
				var val = value ? servicetypeStore.getById(value) : null;
				return val ? val.get('str')
					: '<span class="tvh-grid-unset">Unset</span>';
			}
		}, {
			header : "PMT PID",
			dataIndex : 'pmt',
			width : 50,
			hidden : true
		}, {
			header : "PCR PID",
			dataIndex : 'pcr',
			width : 50,
			hidden : true
		}, actions ]});

	var rec = Ext.data.Record.create([ 'id', 'enabled', 'channelname',
		'interface', 'group', 'port', 'sid', 'pmt', 'pcr', 'stype' ]);

	var store = new Ext.data.JsonStore({
		root : 'entries',
		fields : rec,
		url : "iptv/services",
		autoLoad : true,
		id : 'id',
		baseParams : {
			op : "get"
		},
		listeners : {
			'update' : function(s, r, o) {
				d = s.getModifiedRecords().length == 0
				saveBtn.setDisabled(d);
				rejectBtn.setDisabled(d);
			}
		}
	});

	/*
	 var storeReloader = new Ext.util.DelayedTask(function() {
	store.reload()
	 });

	 tvheadend.comet.on('dvbService', function(m) {
	storeReloader.delay(500);
	 });
	 */

	function addRecord() {
		Ext.Ajax.request({
			url : "iptv/services",
			params : {
				op : "create"
			},
			failure : function(response, options) {
				Ext.MessageBox.alert('Server Error',
					'Unable to generate new record');
			},
			success : function(response, options) {
				var responseData = Ext.util.JSON.decode(response.responseText);
				var p = new rec(responseData, responseData.id);
				grid.stopEditing();
				store.insert(0, p);
				grid.startEditing(0, 0);
			}
		})
	}
	;

	function delSelected() {
		var selectedKeys = grid.selModel.selections.keys;
		if (selectedKeys.length > 0) {
			Ext.MessageBox.confirm('Message',
				'Do you really want to delete selection?', deleteRecord);
		}
		else {
			Ext.MessageBox.alert('Message',
				'Please select at least one item to delete');
		}
	}
	;

	function deleteRecord(btn) {
		if (btn == 'yes') {
			var selectedKeys = grid.selModel.selections.keys;

			Ext.Ajax.request({
				url : "iptv/services",
				params : {
					op : "delete",
					entries : Ext.encode(selectedKeys)
				},
				failure : function(response, options) {
					Ext.MessageBox.alert('Server Error', 'Unable to delete');
				},
				success : function(response, options) {
					store.reload();
				}
			})
		}
	}

	function saveChanges() {
		var mr = store.getModifiedRecords();
		var out = new Array();
		for ( var x = 0; x < mr.length; x++) {
			v = mr[x].getChanges();
			out[x] = v;
			out[x].id = mr[x].id;
		}

		Ext.Ajax.request({
			url : "iptv/services",
			params : {
				op : "update",
				entries : Ext.encode(out)
			},
			success : function(response, options) {
				store.commitChanges();
			},
			failure : function(response, options) {
				Ext.MessageBox.alert('Message', response.statusText);
			}
		});
	}

	var delButton = new Ext.Toolbar.Button({
		tooltip : 'Delete one or more selected rows',
		iconCls : 'remove',
		text : 'Delete selected services',
		handler : delSelected,
		disabled : true
	});

	var saveBtn = new Ext.Toolbar.Button({
		tooltip : 'Save any changes made (Changed cells have red borders).',
		iconCls : 'save',
		text : "Save changes",
		handler : saveChanges,
		disabled : true
	});

	var rejectBtn = new Ext.Toolbar.Button({
		tooltip : 'Revert any changes made (Changed cells have red borders).',
		iconCls : 'undo',
		text : "Revert changes",
		handler : function() {
			store.rejectChanges();
		},
		disabled : true
	});

	var selModel = new Ext.grid.RowSelectionModel({
		singleSelect : false
	});

	var grid = new Ext.grid.EditorGridPanel({
		stripeRows : true,
		title : 'IPTV',
		iconCls : 'iptv',
		plugins : [ enabledColumn, actions ],
		store : store,
		clicksToEdit : 2,
		cm : cm,
		viewConfig : {
			forceFit : true
		},
		selModel : selModel,
		tbar : [
			{
				tooltip : 'Create a new entry on the server. '
					+ 'The new entry is initially disabled so it must be enabled '
					+ 'before it start taking effect.',
				iconCls : 'add',
				text : 'Add service',
				handler : addRecord
			}, '-', delButton, '-', saveBtn, rejectBtn, '->',
			{
				text : 'Help',
				handler : function() {
					new tvheadend.help('IPTV', 'config_iptv.html');
				}
			} ]
	});

	store.on('update', function(s, r, o) {
		d = s.getModifiedRecords().length == 0
		saveBtn.setDisabled(d);
		rejectBtn.setDisabled(d);
	});

	selModel.on('selectionchange', function(self) {
		delButton.setDisabled(self.getCount() == 0);
	});

	return grid;
}
