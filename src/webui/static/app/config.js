// Store: config languages
tvheadend.languages = new Ext.data.JsonStore({
    autoLoad:true,
    root:'entries',
    fields: ['identifier','name'],
    id: 'identifier',
    url:'languages',
    baseParams: {
    	op: 'list'
    }
});

// Store: all languages
tvheadend.config_languages = new Ext.data.JsonStore({
    autoLoad:true,
    root:'entries',
    fields: ['identifier','name'],
    id: 'identifier',
    url:'languages',
    baseParams: {
    	op: 'config'
    }
});

tvheadend.languages.setDefaultSort('name', 'ASC');

tvheadend.comet.on('config', function(m) {
    if(m.reload != null) {
        tvheadend.languages.reload();
        tvheadend.config_languages.reload();
    }
});

tvheadend.miscconf = function() {
	/*
	 * Basic Config
	 */
	var confreader = new Ext.data.JsonReader({
		root : 'config'
	}, [ 'muxconfpath', 'language',
       'imagecache_enabled', 'imagecache_ok_period',
       'imagecache_fail_period']);

	/* ****************************************************************
	 * Form Fields
	 * ***************************************************************/

  /*
   * DVB path
   */

	var dvbscanPath = new Ext.form.TextField({
		fieldLabel : 'DVB scan files path',
		name : 'muxconfpath',
		allowBlank : true,
		width: 400
	});

  /*
   * Language
   */

	var language = new Ext.ux.ItemSelector({
		name: 'language',
		fromStore: tvheadend.languages,
		toStore: tvheadend.config_languages,
		fieldLabel: 'Default Language(s)',
		dataFields:['identifier', 'name'],
		msWidth: 190,
		msHeight: 150,
		valueField: 'identifier',
		displayField: 'name',
		imagePath: 'static/multiselect/resources',
		toLegend: 'Selected',
		fromLegend: 'Available'
	});

  /*
   * Image cache
   */
  var imagecacheEnabled = new Ext.form.Checkbox({
    name: 'imagecache_enabled',
    fieldLabel: 'Enabled',
  });

  var imagecacheOkPeriod = new Ext.form.NumberField({
    name: 'imagecache_ok_period',
    fieldLabel: 'Re-fetch period (hours)'
  });

  var imagecacheFailPeriod = new Ext.form.NumberField({
    name: 'imagecache_fail_period',
    fieldLabel: 'Re-try period (hours)',
  });

  var imagecachePanel = new Ext.form.FieldSet({
    title: 'Image Caching',
    width: 700,
    autoHeight: true,
    collapsible: true,
    items : [ imagecacheEnabled, imagecacheOkPeriod, imagecacheFailPeriod ]
  });
  if (tvheadend.capabilities.indexOf('imagecache') == -1)
    imagecachePanel.hide();

	/* ****************************************************************
	 * Timeshift
	 * ***************************************************************/

	var timeshiftPath = new Ext.form.TextField({
		fieldLabel  : 'Temp. storage path',
		name        : 'timeshiftpath',
		allowBlank  : true,
		width       : 400
	});

	var timeshiftPeriod = new Ext.form.NumberField({
		fieldLabel  : 'Max period (minutes, per stream)',
		name        : 'timeshiftperiod',
		allowBlank  : false,
		width       : 400
	});

	var timeshiftPeriodU = new Ext.form.Checkbox({
		fieldLabel  : '(unlimited)',
		name        : 'timeshiftperiod_unlimited',
		allowBlank  : false,
		width       : 400
	});
	timeshiftPeriodU.on('check', function(e, c) {
		timeshiftPeriod.setDisabled(c);
	});

	var timeshiftSize = new Ext.form.NumberField({
		fieldLabel  : 'Max size (MB, global)',
		name        : 'timeshiftsize',
		allowBlank  : false,
		width       : 400
	});

	var timeshiftFields = new Ext.form.FieldSet({
		title       : 'Timeshift',
		width       : 700,
		autoHeight  : true,
		collapsible : true,
		items       : [ timeshiftPath, timeshiftPeriod, timeshiftPeriodU ]//, timeshiftSize ]
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

	var confpanel = new Ext.form.FormPanel({
		title : 'General',
		iconCls : 'wrench',
		border : false,
		bodyStyle : 'padding:15px',
		labelAlign : 'left',
		labelWidth : 200,
		waitMsgTarget : true,
		reader : confreader,
		layout : 'form',
		defaultType : 'textfield',
		autoHeight : true,
		items : [ language, dvbscanPath,
              imagecachePanel ],
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
				v = parseInt(timeshiftPeriod.getValue());
				if (v == 4294967295) {
					timeshiftPeriodU.setValue(true);
					timeshiftPeriod.setValue("");
					timeshiftPeriod.setDisabled(true); // TODO: this isn't working
				} else {
					timeshiftPeriod.setValue(v / 60);
				}
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
