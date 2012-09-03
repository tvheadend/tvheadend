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

tvheadend.filteraudiosubtitle = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [
		['false', 'Disabled'],
		['true', 'Enabled']
    ]
});

tvheadend.languages.setDefaultSort('name', 'ASC');

tvheadend.comet.on('config', function(m) {
    if(m.reload != null)
        tvheadend.languages.reload();
});

tvheadend.miscconf = function() {

  /*
   * Basic Config
   */

  var confreader = new Ext.data.JsonReader(
    { root: 'config' },
    [ 
      'muxconfpath', 'language', 'filteraudiosubtitle'
    ]
  );

  /* ****************************************************************
   * Form Fields
   * ***************************************************************/

  var dvbscanPath = new Ext.form.TextField({
    fieldLabel: 'DVB scan files path',
    name: 'muxconfpath',
    width: 420,
    allowBlank: true
  });
  
  var language = new Ext.ux.ItemSelector({
	name: 'language',
	fromStore: tvheadend.languages,
	toStore: tvheadend.config_languages,
	fieldLabel: 'Default Language(s)',
	dataFields:['identifier', 'name'],
	msWidth: 200,
	msHeight: 150,
	valueField: 'identifier',
	displayField: 'name',
	imagePath: 'static/multiselect/resources',
	toLegend: 'Selected',
	fromLegend: 'Available'
  });
  
  var filteraudiosubtitle = new Ext.form.ComboBox({
    fieldLabel: 'Filter audio/subtitle by language',
    hiddenName: 'filteraudiosubtitle',
    store: tvheadend.filteraudiosubtitle,
    valueField: 'identifier',
	displayField: 'name',
	forceSelection: true,
	mode:'local',
	triggerAction: 'all',
    width: 150
  });

  /* ****************************************************************
   * Form
   * ***************************************************************/

  var saveButton = new Ext.Button({
    text     : 'Save configuration',
    tooltip  : 'Save changes made to configuration below',
    iconCls  :'save',
    handler  : saveChanges
  });

  var helpButton = new Ext.Button({
    text    : 'Help',
    handler : function() {
      new tvheadend.help('General Configuration',
                         'config_misc.html');
    }
  });

  var confpanel = new Ext.FormPanel({
    title         : 'General',
    iconCls       : 'wrench',
    border        : false,
    bodyStyle     : 'padding:15px',
    labelAlign    : 'left',
    labelWidth    : 190,
    waitMsgTarget : true,
    reader        : confreader,
    layout        : 'form',
    defaultType   : 'textfield',
    autoHeight    : true,
    items         : [
      language,
      filteraudiosubtitle,
      dvbscanPath
    ],
    tbar: [
      saveButton,
      '->',
      helpButton
    ]
  });

  /* ****************************************************************
   * Load/Save
   * ***************************************************************/

  confpanel.on('render', function() {
    confpanel.getForm().load({
      url     : 'config',
      params  : { op : 'loadSettings' },
      success : function ( form, action ) {
        confpanel.enable();
      }
    });
  });

  function saveChanges() {
    confpanel.getForm().submit({
      url     : 'config', 
      params  : { op : 'saveSettings' },
      waitMsg : 'Saving Data...',
      failure : function (form, action) {
        Ext.Msg.alert('Save failed', action.result.errormsg);
      }
    });
  }

  return confpanel;
}
