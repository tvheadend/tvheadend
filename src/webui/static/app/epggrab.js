tvheadend.epggrabChannels = new Ext.data.JsonStore({
	root : 'entries',
	url : 'epggrab',
	baseParams : {
		op : 'channelList'
	},
	fields : [ 'id', 'mod', 'name', 'icon', 'number', 'channel', 'mod-id',
		'mod-name' ]
});

tvheadend.epggrab = function() {

	/* ****************************************************************
	 * Data
	 * ***************************************************************/

	/*
	 * Module lists (I'm sure there is a better way!)
	 */
	var EPGGRAB_MODULE_INTERNAL = 0;
	var EPGGRAB_MODULE_EXTERNAL = 1;
	var EPGGRAB_MODULE_OTA = 2;

	var moduleStore = new Ext.data.JsonStore({
		root : 'entries',
		url : 'epggrab',
		baseParams : {
			op : 'moduleList'
		},
		autoLoad : true,
		fields : [ 'id', 'name', 'path', 'type', 'enabled' ]
	});
	var internalModuleStore = new Ext.data.Store({
		recordType : moduleStore.recordType
	});
	var externalModuleStore = new Ext.data.Store({
		recordType : moduleStore.recordType
	});
	var otaModuleStore = new Ext.data.Store({
		recordType : moduleStore.recordType
	});
	moduleStore.on('load', function() {
		moduleStore.filterBy(function(r) {
			return r.get('type') == EPGGRAB_MODULE_INTERNAL;
		});
		r = new internalModuleStore.recordType({
			id : '',
			name : 'Disabled'
		});
		internalModuleStore.add(r);
		moduleStore.each(function(r) {
			internalModuleStore.add(r.copy());
		});
		moduleStore.filterBy(function(r) {
			return r.get('type') == EPGGRAB_MODULE_EXTERNAL;
		});
		moduleStore.each(function(r) {
			externalModuleStore.add(r.copy());
		});
		moduleStore.filterBy(function(r) {
			return r.get('type') == EPGGRAB_MODULE_OTA;
		});
		moduleStore.each(function(r) {
			otaModuleStore.add(r.copy());
		});
		moduleStore.filterBy(function(r) {
			return r.get('type') != EPGGRAB_MODULE_INTERNAL;
		});
	});

	/* Enable module in one of the stores (will auto update primary) */
	function moduleSelect(r, e) {
		r.set('enabled', e);
		t = moduleStore.getById(r.id);
		if (t) t.set('enabled', e);
	}

	/*
	 * Basic Config
	 */

	var confreader = new Ext.data.JsonReader({
		root : 'epggrabSettings'
	}, [ 'module', 'interval', 'channel_rename', 'channel_renumber',
		'channel_reicon', 'epgdb_periodicsave' ]);

	/* ****************************************************************
	 * Basic Fields
	 * ***************************************************************/

	/*
	 * Module selector
	 */
	var internalModule = new Ext.form.ComboBox({
		fieldLabel : 'Module',
		hiddenName : 'module',
		width : 300,
		valueField : 'id',
		displayField : 'name',
		forceSelection : true,
		editable : false,
		mode : 'local',
		triggerAction : 'all',
		store : internalModuleStore
	});

	/*
	 * Interval selector
	 */
	var intervalUnits = [ [ 86400, 'Days' ], [ 3600, 'Hours' ],
		[ 60, 'Minutes' ], [ 1, 'Seconds' ] ];
	var intervalValue = new Ext.form.NumberField({
		width : 300,
		allowNegative : false,
		allowDecimals : false,
		minValue : 1,
		maxValue : 7,
		value : 1,
		fieldLabel : 'Grab interval',
		name : 'intervalValue',
		listeners : {
			'valid' : function(e) {
				v = e.getValue() * intervalUnit.getValue();
				interval.setValue(v);
			}
		}
	})
	var intervalUnit = new Ext.form.ComboBox({
		name : 'intervalUnit',
		width : 300,
		valueField : 'key',
		displayField : 'value',
		value : 86400,
		forceSelection : true,
		editable : false,
		triggerAction : 'all',
		mode : 'local',
		store : new Ext.data.SimpleStore({
			fields : [ 'key', 'value' ],
			data : intervalUnits
		}),
		listeners : {
			'change' : function(e, n, o) {
				intervalValue.maxValue = (7 * 86400) / n;
				intervalValue.validate();
			}
		}
	});
	var interval = new Ext.form.Hidden({
		name : 'interval',
		value : 86400,
		listeners : {
			'enable' : function(e) {
				v = e.getValue();
				for (i = 0; i < intervalUnits.length; i++) {
					u = intervalUnits[i][0];
					if ((v % u) == 0) {
						intervalUnit.setValue(u);
						intervalValue.maxValue = (7 * 86400) / u;
						intervalValue.setValue(v / u);
						intervalValue.validate();
						break;
					}
				}
			}
		}
	});

	/*
	 * Channel handling
	 */
	var channelRename = new Ext.form.Checkbox({
		name : 'channel_rename',
		fieldLabel : 'Update channel name'
	});

	var channelRenumber = new Ext.form.Checkbox({
		name : 'channel_renumber',
		fieldLabel : 'Update channel number'
	});

	var channelReicon = new Ext.form.Checkbox({
		name : 'channel_reicon',
		fieldLabel : 'Update channel icon'
	});

	var epgPeriodicSave = new Ext.form.Checkbox({
		name : 'epgdb_periodicsave',
		fieldLabel : 'Periodic save EPG to disk'
	});

	/*
	 * Simple fields
	 */
	var simplePanel = new Ext.form.FieldSet({
		title : 'General Config',
		width : 700,
		autoHeight : true,
		collapsible : true,
		items : [ channelRename, channelRenumber, channelReicon, epgPeriodicSave ]
	});

	/*
	 * Internal grabber
	 */
	var internalPanel = new Ext.form.FieldSet({
		title : 'Internal Grabber',
		width : 700,
		autoHeight : true,
		collapsible : true,
		items : [ interval, internalModule, intervalValue, intervalUnit ]
	});

	/* ****************************************************************
	 * Advanced Fields
	 * ***************************************************************/

	/*
	 * External modules
	 */
	var externalSelectionModel = new Ext.grid.CheckboxSelectionModel({
		singleSelect : false,
		listeners : {
			'rowselect' : function(s, ri, r) {
				moduleSelect(r, 1);
			},
			'rowdeselect' : function(s, ri, r) {
				moduleSelect(r, 0);
			}
		}
	});

	var externalColumnModel = new Ext.grid.ColumnModel([ externalSelectionModel,
		{
			header : 'Module',
			dataIndex : 'name',
			width : 200,
			sortable : false
		}, {
			header : 'Path',
			dataIndex : 'path',
			width : 300,
			sortable : false
		} ]);

	var externalGrid = new Ext.grid.EditorGridPanel({
		store : externalModuleStore,
		cm : externalColumnModel,
		sm : externalSelectionModel,
		width : 600,
		height : 150,
		frame : false,
		viewConfig : {
			forceFit : true
		},
		iconCls : 'icon-grid'
	});

	var externalPanel = new Ext.form.FieldSet({
		title : 'External Interfaces',
		width : 700,
		autoHeight : true,
		collapsible : true,
		collapsed : true,
		items : [ externalGrid ]
	});

	/*
	 * OTA modules
	 */

	var otaSelectionModel = new Ext.grid.CheckboxSelectionModel({
		singleSelect : false,
		listeners : {
			'rowselect' : function(s, ri, r) {
				moduleSelect(r, 1);
			},
			'rowdeselect' : function(s, ri, r) {
				moduleSelect(r, 0);
			}
		}
	});

	var otaColumnModel = new Ext.grid.ColumnModel([ otaSelectionModel, {
		header : 'Module',
		dataIndex : 'name',
		width : 200,
		sortable : false
	} ]);

	var otaGrid = new Ext.grid.EditorGridPanel({
		store : otaModuleStore,
		cm : otaColumnModel,
		sm : otaSelectionModel,
		width : 600,
		height : 150,
		frame : false,
		viewConfig : {
			forceFit : true
		},
		iconCls : 'icon-grid'
	});

	var otaPanel = new Ext.form.FieldSet({
		title : 'Over-the-air Grabbers',
		width : 700,
		autoHeight : true,
		collapsible : true,
		collapsed : true,
		items : [ otaGrid ]
	});

	/* ****************************************************************
	 * Form
	 * ***************************************************************/

	var saveButton = new Ext.Button({
		text : "Save configuration",
		tooltip : 'Save changes made to configuration below',
		iconCls : 'save',
		handler : saveChanges
	});

	var helpButton = new Ext.Button({
		text : 'Help',
		handler : function() {
			new tvheadend.help('EPG Grab Configuration', 'config_epggrab.html');
		}
	});

	var confpanel = new Ext.FormPanel({
		title : 'EPG Grabber',
		iconCls : 'xml',
		border : false,
		bodyStyle : 'padding:15px',
		labelAlign : 'left',
		labelWidth : 150,
		waitMsgTarget : true,
		reader : confreader,
		layout : 'form',
		defaultType : 'textfield',
		autoHeight : true,
		items : [ simplePanel, internalPanel, otaPanel, externalPanel ],
		tbar : [ saveButton, '->', helpButton ]
	});

	/* ****************************************************************
	 * Load/Save
	 * ***************************************************************/

	/* HACK: get display working */
	externalGrid.on('render', function() {
		delay = new Ext.util.DelayedTask(function() {
			rows = [];
			externalModuleStore.each(function(r) {
				if (r.get('enabled')) rows.push(r);
			});
			externalSelectionModel.selectRecords(rows);
		});
		delay.delay(100);
	});
	otaGrid.on('render', function() {
		delay = new Ext.util.DelayedTask(function() {
			rows = [];
			otaModuleStore.each(function(r) {
				if (r.get('enabled')) rows.push(r);
			});
			otaSelectionModel.selectRecords(rows);
		});
		delay.delay(100);
	});

	confpanel.on('render', function() {

		/* Hack to get display working */
		delay = new Ext.util.DelayedTask(function() {
			simplePanel.doLayout(false);
			internalPanel.doLayout(false);
			externalPanel.doLayout(false);
			otaPanel.doLayout(false);
		});
		delay.delay(100);

		confpanel.getForm().load({
			url : 'epggrab',
			params : {
				op : 'loadSettings'
			},
			success : function(form, action) {
				confpanel.enable();
			}
		});
	});

	function saveChanges() {
		mods = [];
		moduleStore.each(function(r) {
			mods.push({
				id : r.get('id'),
				enabled : r.get('enabled') ? 1 : 0
			});
		});
		mods = Ext.util.JSON.encode(mods);
		confpanel.getForm().submit({
			url : 'epggrab',
			params : {
				op : 'saveSettings',
				external : mods
			},
			waitMsg : 'Saving Data...',
			success : function(form, action) {
				externalModuleStore.commitChanges();
			},
			failure : function(form, action) {
				Ext.Msg.alert('Save failed', action.result.errormsg);
			}
		});
	}

	return confpanel;
}
