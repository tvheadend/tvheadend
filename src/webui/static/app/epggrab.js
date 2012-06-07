tvheadend.epggrab = function() {

  /* ****************************************************************
   * Data
   * ***************************************************************/

  /*
   * Module lists (I'm sure there is a better way!)
   */
  var EPGGRAB_MODULE_SIMPLE   = 0x01;
  var EPGGRAB_MODULE_EXTERNAL = 0x02;

  var moduleStore = new Ext.data.JsonStore({
    root       : 'entries',
    url        : 'epggrab',
    baseParams : { op : 'moduleList' },
    autoLoad   : true,
    fields     : [ 'id', 'name', 'path', 'flags', 'enabled' ]
  });
  var simpleModuleStore = new Ext.data.Store({
    recordType: moduleStore.recordType
  });
  var externalModuleStore = new Ext.data.Store({
    recordType: moduleStore.recordType
  });
  moduleStore.on('load', function() {
    moduleStore.filterBy(function(r) {
      return r.get('flags') & EPGGRAB_MODULE_SIMPLE;
    });
    r = new simpleModuleStore.recordType({ id: '', name : 'Disabled'});
    simpleModuleStore.add(r);
    moduleStore.each(function(r) {
      simpleModuleStore.add(r.copy());
    });
    moduleStore.filterBy(function(r) {
      return r.get('flags') & EPGGRAB_MODULE_EXTERNAL;
    });
    moduleStore.each(function(r) {
      externalModuleStore.add(r.copy());
    });
  });

  /*
   * Basic Config
   */

  var confreader = new Ext.data.JsonReader(
    { root: 'epggrabSettings' },
    [ 'module', 'eitenabled', 'advanced', 'interval' ]
  );

  /* ****************************************************************
   * Basic Fields
   * ***************************************************************/

  /*
   * Module selector
   */
  var simpleModule = new Ext.form.ComboBox({
    fieldLabel     : 'Module',
    hiddenName     : 'module',
    width          : 300,
    valueField     : 'id',
    displayField   : 'name',
    forceSelection : true,
    editable       : false,
    mode           : 'local',
    triggerAction  : 'all',
    store          : simpleModuleStore
  });

  /*
   * Interval selector
   */
  var intervalUnits = [
    [ 86400, 'Days'    ],
    [ 3600,  'Hours'   ],
    [ 60,    'Minutes' ],
    [ 1,     'Seconds' ]
  ];
  var intervalValue = new Ext.form.NumberField({
    width         : 300,
    allowNegative : false,
    allowDecimals : false,
    minValue      : 1,
    maxValue      : 7,
    value         : 1,
    fieldLabel    : 'Grab interval',
    name          : 'intervalValue',
    listeners      : {
      'valid' : function (e) {
        v = e.getValue() * intervalUnit.getValue();
        interval.setValue(v);
      }
    }
  })
  var intervalUnit = new Ext.form.ComboBox({
    name           : 'intervalUnit',
    width          : 300,
    valueField     : 'key',
    displayField   : 'value',
    value          : 86400,
    forceSelection : true,
    editable       : false,
    triggerAction  : 'all',
    mode           : 'local',
    store          : new Ext.data.SimpleStore({
      fields : [ 'key', 'value' ],
      data   : intervalUnits
    }),
    listeners      : {
      'change' : function (e, n, o) {
        intervalValue.maxValue = (7 * 86400) / n;
        intervalValue.validate();
      }
    }
  });
  var interval = new Ext.form.Hidden({
    name  : 'interval',
    value : 86400,
    listeners : {
      'enable' : function (e) {
        v = e.getValue();
        for ( i = 0; i < intervalUnits.length; i++ ) {
          u = intervalUnits[i][0];
          if ( (v % u) == 0 ) {
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

  /* ****************************************************************
   * Advanced Fields
   * ***************************************************************/

  /*
   * External modules
   */
  var externalColumnModel = new Ext.grid.ColumnModel([
    {
      header    : 'Module',
      dataIndex : 'name',
      width     : 200,
      sortable  : false,
    },
    {
      header    : 'Path',
      dataIndex : 'path',
      width     : 300,
      sortable  : false,
      // TODO: editable?
    },
    {
      dataIndex : 'enabled',
      header    : 'Enabled',
      width     : 100,
      sortable  : false,
      editor    : new Ext.form.Checkbox(),
      // TODO: newer versions of extjs provide proper checkbox in grid
      //       support, I think!
      renderer  : function (v) {
        if (v) {
          return "Y";
        } else {
          return "N";
        }
      }
    }
  ]);

  var externalGrid = new Ext.grid.EditorGridPanel({
    store          : externalModuleStore,
    cm             : externalColumnModel,
    width          : 600,
    height         : 150,
    frame          : false,
    viewConfig     : {
      forceFit  : true,
    },
    iconCls        : 'icon-grid',
  });
  var advancedPanel = externalGrid;

  /* ****************************************************************
   * Form
   * ***************************************************************/

  var eitCheck = new Ext.form.Checkbox({
    fieldLabel    : 'EIT Enabled',
    name          : 'eitenabled'
  });

  var saveButton = new Ext.Button({
    text     : "Save configuration",
    tooltip  : 'Save changes made to configuration below',
    iconCls  :'save',
    handler  : saveChanges,
  });

  var helpButton = new Ext.Button({
    text    : 'Help',
    handler : function() {
      alert('TODO: help info');
    }
  });

  var confpanel = new Ext.FormPanel({
    title         : 'EPG Grabber',
    iconCls       : 'xml',
    border        : false,
    bodyStyle     : 'padding:15px',
    labelAlign    : 'left',
    labelWidth    : 150,
    waitMsgTarget : true,
    reader        : confreader,
    layout        : 'form',
    defaultType   : 'textfield',
    items         : [
      interval,
      eitCheck,
      simpleModule,
      intervalValue,
      intervalUnit,
      new Ext.form.Label({text: 'External Interfaces'}),
      advancedPanel
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
      url     : 'epggrab',
      params  : { op : 'loadSettings' },
      success : function ( form, action ) {
        confpanel.enable();
      }
    });
  });

  function saveChanges() {
    mods = [];
    externalModuleStore.each(function(r) {
      mods.push({id: r.get('id'), enabled: r.get('enabled') ? 1 : 0});
    });  
    mods = Ext.util.JSON.encode(mods);
    confpanel.getForm().submit({
      url     : 'epggrab', 
      params  : { op : 'saveSettings', external : mods },
      waitMsg : 'Saving Data...',
      success : function(form, action) {
        externalModuleStore.commitChanges();
      },
      failure : function (form, action) {
        Ext.Msg.alert('Save failed', action.result.errormsg);
      }
    });
  }

  return confpanel;
}

