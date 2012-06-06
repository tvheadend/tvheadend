tvheadend.epggrab = function() {

  /* ****************************************************************
   * Data
   * ***************************************************************/

  /*
   * Module lists (I'm sure there is a better way!)
   */
  var EPGGRAB_MODULE_SIMPLE   = 0x04;
  var EPGGRAB_MODULE_ADVANCED = 0x08;
  var EPGGRAB_MODULE_EXTERNAL = 0x10;

  var moduleStore = new Ext.data.JsonStore({
    root       : 'entries',
    url        : 'epggrab',
    baseParams : { op : 'moduleList' },
    autoLoad   : true,
    fields     : [ 'id', 'name', 'path', 'flags' ]
  });
  var simpleModuleStore = new Ext.data.Store({
    recordType: moduleStore.recordType
  });
  var advancedModuleStore = new Ext.data.Store({
    recordType: moduleStore.recordType
  });
  var externalModuleStore = new Ext.data.Store({
    recordType: moduleStore.recordType
  });
  moduleStore.on('load', function() {
    moduleStore.filterBy(function(r) {
      return r.get('flags') & EPGGRAB_MODULE_SIMPLE;
    });
    moduleStore.each(function(r) {
      simpleModuleStore.add(r.copy());
    });
    moduleStore.filterBy(function(r) {
      return r.get('flags') & (EPGGRAB_MODULE_ADVANCED | EPGGRAB_MODULE_SIMPLE);
    });
    moduleStore.each(function(r) {
      advancedModuleStore.add(r.copy());
    });
    moduleStore.filterBy(function(r) {
      return r.get('flags') & EPGGRAB_MODULE_EXTERNAL;
    });
    moduleStore.each(function(r) {
      externalModuleStore.add(r.copy());
    });
  });

  /*
   * Schedule
   */

  var scheduleRow = Ext.data.Record.create(
    [
      { name : 'module'  },
      { name : 'command' },
      { name : 'options' },
      { name : 'cron'    }
    ]
  );

  var scheduleStore = new Ext.data.JsonStore({
    root       : 'entries',
    url        : 'epggrab',
    baseParams : { op : 'loadSchedule' },
    autoLoad   : true,
    fields     : [ 'module', 'command', 'options', 'cron' ]
  });

  /*
   * Basic Config
   */

  var confreader = new Ext.data.JsonReader(
    { root: 'epggrabSettings' },
    [ 'module', 'eitenabled', 'advanced', 'interval' ]
  );

  /* ****************************************************************
   * Simple Config
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

  var simplePanel = new Ext.form.FieldSet({
    title  : 'Simple Config',
    height : 120,
    width  : 900,
    items : [
      simpleModule,
      intervalValue,
      intervalUnit
    ]
  });

  /* ****************************************************************
   * Advanced Config
   * ***************************************************************/

  /*
   * Schedule
   */
  var scheduleColumnModel = new Ext.grid.ColumnModel([
    {
      header    : 'Module',
      dataIndex : 'module',
      width     : 300,
      sortable  : false,
      renderer  : function (v)
      {
        if ( v != "" ) {
          i = advancedModuleStore.find('id', v);
          v = advancedModuleStore.getAt(i).get('name');
        }
        return v;
      },
      editor    : new Ext.form.ComboBox({
        valueField    : 'id',
        displayField  : 'name',
        editable      : false,
        mode          : 'local',
        triggerAction : 'all',
        store         : advancedModuleStore
      }),
    },
    {
      header : 'Cron',
      dataIndex : 'cron',
      width     : 150,
      sortable  : false,
      editor    : new Ext.form.TextField()
    },
    {
      header    : 'Command',
      dataIndex : 'command',
      width     : 200,
      sortable  : false,
      editor    : new Ext.form.TextField()
    },
    {
      dataIndex : 'options',
      header    : 'Options',
      width     : 200,
      sortable  : false,
      editor    : new Ext.form.TextField()
    }
  ]);
  scheduleColumnModel.isCellEditable = function (ci, ri)
  {
    if (ci == 0) return true;
    m = scheduleStore.getAt(ri).get('module');
    if (m == "") return false;
    m = advancedModuleStore.find('id', m);
    m = advancedModuleStore.getAt(m);
    if (!(m.get('flags') & EPGGRAB_MODULE_ADVANCED)) {
      return (ci == 1);
    }
    return true;
  };

  scheduleSelectModel = new Ext.grid.RowSelectionModel({
    singleSelect : false,
  });
  scheduleSelectModel.on('selectionchange', function(s) {
    delButton.setDisabled(s.getCount() == 0);
  });

  var addButton = new Ext.Button({
    text    : 'Add',
    iconCls : 'add',
    handler : function () {
      scheduleStore.add(new scheduleRow({
        module  : '',
        cron    : '',
        command : '',
        options : ''}
      ));
    }
  });
  
  var delButton = new Ext.Button({
    text     : 'Delete',
    iconCls  : 'remove',
    disabled : true,
    handler : function () {
      var s = schedulePanel.getSelectionModel().each(function(r){
        scheduleStore.remove(r);
      });
    }
  });

  var schedulePanel = new Ext.grid.EditorGridPanel({
    store          : scheduleStore,
    cm             : scheduleColumnModel,
    sm             : scheduleSelectModel,
    width          : 850,
    height         : 150,
    frame          : true,
    viewConfig     : {
      forceFit  : true,
      markDirty : false
    },
    iconCls        : 'icon-grid',
    tbar : [
      addButton,
      delButton
    ],
    listeners : {
      'afteredit' : function (r) {
        if ( r.field == 'module' ) {
          d = scheduleStore.getAt(r.row);
          c = '';
          if ( r.value != "" ) {
            i = advancedModuleStore.find('id', r.value);
            m = advancedModuleStore.getAt(i);
            c = m.get('path');
          }
          d.set('command', c)
        }
      },
      'select' : function(r) {
        delButton.setDisabled(false);
      }
    }
  });

  var advancedPanel = new Ext.form.FieldSet({
    title : 'Advanced Config',
    height : 200,
    width  : 900,
    items  : [
      // TODO: external editors
      schedulePanel
    ]
  });

  /* ****************************************************************
   * Form
   * ***************************************************************/

  var advancedCheck = new Ext.form.Checkbox({
    fieldLabel    : 'Advanced Config',
    name          : 'advanced',
    listeners     : {
      'check' : function (e, v) {
        simplePanel.setVisible(!v);
        advancedPanel.setVisible(v);
      }
    }
  });

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
      advancedCheck,
      eitCheck,
      simplePanel,
      advancedPanel
    ],
    tbar: [
      saveButton,
      '->',
      helpButton
    ]
  });

  // TODO: HACK: bug in extjs seems to cause sub-components of the form not to render!
  confpanel.on('afterlayout', function()
  {
    simplePanel.syncSize();
    advancedPanel.syncSize();
    simplePanel.setVisible(!advancedCheck.getValue());
    advancedPanel.setVisible(advancedCheck.getValue());
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
    data  = [];
    scheduleStore.each(function(r) {
      data.push(r.data);
    });
    json = Ext.util.JSON.encode(data);
    confpanel.getForm().submit({
      url     : 'epggrab', 
      params  : { op : 'saveSettings', schedule : json },
      waitMsg : 'Saving Data...',
      success : function(form, action) {
        scheduleStore.commitChanges();
      },
      failure : function (form, action) {
        Ext.Msg.alert('Save failed', action.result.errormsg);
      }
    });
  }

  return confpanel;
}

