tvheadend.miscconf = function() {

  /*
   * Basic Config
   */

  var confreader = new Ext.data.JsonReader(
    { root: 'config' },
    [ 
      'muxconfpath',
    ]
  );

  /* ****************************************************************
   * Form Fields
   * ***************************************************************/

  var dvbscanPath = new Ext.form.TextField({
    fieldLabel : 'DVB scan files path',
    name       : 'muxconfpath',
    allowBlank : true,
  });

  /* ****************************************************************
   * Form
   * ***************************************************************/

  var saveButton = new Ext.Button({
    text     : "Save configuration",
    tooltip  : 'Save changes made to configuration below',
    iconCls  :'save',
    handler  : saveChanges,
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
