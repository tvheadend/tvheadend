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
