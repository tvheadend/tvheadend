tvheadend.filteraudiosubtitle = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [
		['false', 'Disabled'],
		['true', 'Enabled']
    ]
});

tvheadend.miscconf = function() {
	/*
	 * Basic Config
	 */
	var confreader = new Ext.data.JsonReader({
		root : 'config'
	}, [ 'muxconfpath', 'language', 'filteraudiosubtitle' ]);

	/* ****************************************************************
	 * Form Fields
	 * ***************************************************************/

	var dvbscanPath = new Ext.form.TextField({
		fieldLabel : 'DVB scan files path',
		name : 'muxconfpath',
		allowBlank : true
	});

	var language = new Ext.form.TextField({
		fieldLabel : 'Default Language(s)',
		name : 'language',
		allowBlank : true
	});
	
	var filteraudiosubtitle = new Ext.form.ComboBox({
		fieldLabel: 'Filter audio/subtitle by language',
    	hiddenName: 'filteraudiosubtitle',
    	store: tvheadend.filteraudiosubtitle,
    	valueField: 'identifier',
    	displayField: 'name',
    	editable: false,
		forceSelection: true,
		mode:'local',
		valueNotFoundText: 'Disabled',
		triggerAction: 'all',
    	width: 150
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
			new tvheadend.help('General Configuration', 'config_misc.html');
		}
	});

	var confpanel = new Ext.FormPanel({
		title : 'General',
		iconCls : 'wrench',
		border : false,
		bodyStyle : 'padding:15px',
		labelAlign : 'left',
		labelWidth : 190,
		waitMsgTarget : true,
		reader : confreader,
		layout : 'form',
		defaultType : 'textfield',
		autoHeight : true,
		items : [ language, dvbscanPath, filteraudiosubtitle ],
		tbar : [ saveButton, '->', helpButton ]
	});

	/* ****************************************************************
	 * Load/Save
	 * ***************************************************************/

	confpanel.on('render', function() {
		confpanel.getForm().load({
			url : 'config',
			params : {
				op : 'loadSettings'
			},
			success : function(form, action) {
				confpanel.enable();
			}
		});
	});

	function saveChanges() {
		confpanel.getForm().submit({
			url : 'config',
			params : {
				op : 'saveSettings'
			},
			waitMsg : 'Saving Data...',
			failure : function(form, action) {
				Ext.Msg.alert('Save failed', action.result.errormsg);
			}
		});
	}

	return confpanel;
}
