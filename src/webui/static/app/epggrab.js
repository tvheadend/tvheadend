tvheadend.grabberStore = new Ext.data.JsonStore({
    root:'entries',
    fields: ['identifier','name','version','apiconfig'],
    url:'xmltv',
    baseParams: {
	op: 'listGrabbers'
    }
});

tvheadend.epggrab = function() {

  var confreader = new Ext.data.JsonReader(
    { root: 'epggrabSettings' },
    [ 'module', 'eitenable', 'advanced' ]
  );

  var grabberSelect = new Ext.form.ComboBox({
	loadingText: 'Loading, please wait...',
	fieldLabel: 'XML-TV Source',
	name: 'grabber',
	width: 350,
	displayField:'name',
	valueField:'identifier',
	store: tvheadend.grabberStore,
	forceSelection: true,
	editable: false,
	triggerAction: 'all',
	mode: 'remote',
	emptyText: 'Select grabber'
    });
  grabberSelect.setVisible(false);

  var advancedCheck = new Ext.form.Checkbox({
	  fieldLabel    : 'Advanced Config',
		name          : 'advanced',
	});
  
  var moduleSelect = new Ext.form.ComboBox({
    fieldLabel     : 'Grab Module',
    name           : 'module',
    width          : 150,
    valueField     : 'key',
    displayField   : 'value',
    forceSelection : true,
    editable       : false,
    triggerAction  : 'all',
    mode           : 'local',
    store          : new Ext.data.SimpleStore({
      fields  : [ 'key', 'value' ],
      data    : [
        [ 'xmltv',  'XMLTV'   ],
        [ 'pyepg',  'PyEPG'   ],
      ]
    })
  });

  var confpanel = new Ext.FormPanel(
    {
	    title         : 'EPG Grabber',
    	iconCls       : 'xml',
	    border        : false,
    	bodyStyle     : 'padding:15px',
	    labelAlign    : 'right',
    	labelWidth    : 200,
	    waitMsgTarget : true,
    	reader        : confreader,
	    layout        : 'form',
  	  defaultType   : 'textfield',
  	  items         : [
  	    moduleSelect,
        new Ext.form.Checkbox({
		      fieldLabel    : 'Enable EIT',
		      name          : 'eitenable'
	      }),
        advancedCheck,
        grabberSelect
	    ],
	    tbar: [
        {
	        tooltip: 'Save changes made to configuration below',
	        iconCls:'save',
	        text: "Save configuration",
	        handler: saveChanges
	      },
        '->',
        {
	        text: 'Help',
	        handler: function() {
		        new tvheadend.help('XMLTV configuration', 'config_xmltv.html');
	        }
	      }
      ]
    }
  );

  /* 
   * Event handlers
   */
  advancedCheck.on('enable', function(e) {
    Ext.MessageBox.alert('Test', 'testing');
  });

  moduleSelect.on('select', function(c,r,i) {
    alert('select module');
  });

    confpanel.on('render', function() {
	confpanel.getForm().load({
	    url:'xmltv', 
	    params:{'op':'loadSettings'},
	    success:function(form, action) {
		confpanel.enable();
	    }
	});
    });


    grabberSelect.on('select', function(c,r,i) {

	    Ext.MessageBox.alert('XMLTV',
				 'Make sure that the grabber is properly ' +
				 'configured before saving configuration.<br>'+
				 '<br>' +
				 'To configure manually execute the ' +
				 'following command in a shell on the ' +
				 'server:<br>' +
				 '$ ' + r.data.identifier + 
				 ' --configure<br>' +
				 '<br>' +
				 'Note: It is important to configure the ' +
				 'grabber using the same userid as tvheadend '+
				 'since most grabbers save their '+
				 'configuration in the users home directory.'+
				 '<br>' +
				 '<br>' +
				 'Grabber version: ' + r.data.version
				);

/*
	if(r.data.apiconfig) {

	    Ext.MessageBox.confirm('XMLTV',
				   'Configure grabber? ' +
				   'If you know that the grabber is already '+
				   'set up or if you want to configure it '+
				   'manually you may skip this step',
				   function(button) {
				       Ext.MessageBox.alert('XMLTV',
							    'oops, embedded '+
							    'config not '+
							    'implemeted yet');
				   }
				  );

	} else {
	    Ext.MessageBox.alert('XMLTV',
				 'This grabber does not support being ' +
				 'configured from external application ' +
				 '(such as Tvheadend).<br>' +
				 'Make sure that the grabber is properly ' +
				 'configured before saving configuration.<br>'+
				 '<br>' +
				 'To configure manually execute the ' +
				 'following command in a shell on the ' +
				 'server:<br>' +
				 '$ ' + r.data.identifier + 
				 ' --configure<br>' +
				 '<br>' +
				 'Note: It is important to configure the ' +
				 'grabber using the same userid as tvheadend '+
				 'since most grabbers save their '+
				 'configuration in the users home directory.'+
				 '<br>' +
				 '<br>' +
				 'Grabber version: ' + r.data.version
				);
	}
*/
    });

    function saveChanges() {
	confpanel.getForm().submit({
	    url:'xmltv', 
	    params:{'op':'saveSettings'},
	    waitMsg:'Saving Data...',
	    failure: function(form, action) {
		Ext.Msg.alert('Save failed', action.result.errormsg);
	    }
	});
    }

    return confpanel;
}

