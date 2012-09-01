/**
 * Configuration names
 */
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
      'muxconfpath', 'language'
    ]
  );

  /* ****************************************************************
   * Form Fields
   * ***************************************************************/

  var dvbscanPath = new Ext.form.TextField({
    fieldLabel : 'DVB scan files path',
    name       : 'muxconfpath',
    allowBlank : true
  });
  
  var language = new Ext.ux.form.LovCombo({
	store: tvheadend.languages,
	name: 'language',
	mode: 'local',
	width: 400,
	triggerAction:'all',
	fieldLabel: 'Default Language(s)',
    valueField: 'identifier',
    displayField: 'name'
  });

  /* ****************************************************************
   * Form
   * ***************************************************************/

  var saveButton = new Ext.Button({
    text     : "Save configuration",
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
    labelWidth    : 150,
    waitMsgTarget : true,
    reader        : confreader,
    layout        : 'form',
    defaultType   : 'textfield',
    autoHeight    : true,
    items         : [
      language,
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
      params  : { op : 'saveSettings', language : language.getValue() },
      waitMsg : 'Saving Data...',
      failure : function (form, action) {
        Ext.Msg.alert('Save failed', action.result.errormsg);
      }
    });
  }

  return confpanel;
}
