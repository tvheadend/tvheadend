json_decode = function(d)
{
  if (d && d.responseText) {
    d = Ext.util.JSON.decode(d.responseText)
    d = d.entries;
  } else {
    d = []
  }
  return d;
}

/*
 * IDnode creation dialog
 */
tvheadend.idnode_create = function(conf)
{
  var puuid = null;
  var panel = null;

  /* Buttons */
  var saveBtn = new Ext.Button({
    tooltip     : 'Create new entry',
    text        : 'Create',
    hidden      : true,
    handler     : function(){
      params = conf.create.params || {}
      params['uuid'] = puuid;
      params['conf'] = Ext.util.JSON.encode(panel.getForm().getValues());
      Ext.Ajax.request({
        url    : conf.create.url || conf.url,
        params : params,
        success : function(d) {
          win.close();
        }
      });
    }
  });
  var undoBtn = new Ext.Button({
    tooltip     : 'Cancel operation',
    text        : 'Cancel',
    handler     : function(){
      win.close();
    }
  });

  /* Form */
  panel = new Ext.FormPanel({
    frame       : true,
    border      : true,
    bodyStyle   : 'padding: 5px',
    labelAlign  : 'left',
    labelWidth  : 200,
    autoWidth   : true,
    autoHeight  : true,
    defaultType : 'textfield',
    buttonAlign : 'left',
    items       : [],
    buttons     : [ undoBtn, saveBtn ]
  });

  /* Create window */
  win = new Ext.Window({
    title       : 'Add ' + conf.title,
    layout      : 'fit',
    autoWidth   : true,
    autoHeight  : true,
    plain       : true,
    items       : panel
  });

  /*
   * Build the edit panel
   */
  function build_form (d)
  {
    saveBtn.setVisible(true);

    /* Fields */
    for (i = 0; i < d.length; i++) {
      if (d[i].rdonly) continue;
      if (d[i].type == 'int' || d[i].type == 'u16' || d[i].type == 'u32') {
        panel.add(new Ext.form.NumberField({
          fieldLabel : d[i].caption,
          name       : d[i].id,
          width      : 300
        }));
      } else if (d[i].type == 'bool') {
        panel.add(new Ext.form.Checkbox({
          fieldLabel : d[i].caption,
          name       : d[i].id
        }));
      } else if (d[i].type == 'str') {
        panel.add(new Ext.form.TextField({
          fieldLabel : d[i].caption,
          name       : d[i].id,
          width      : 300
        }));
      }
    }
    panel.doLayout();
  }

  /* Do we need to first select a class? */
  if (conf.select) {

    /* Parent selector */
    var combo = new Ext.form.ComboBox({
      fieldLabel    : conf.select.label,
      grow          : true,
      editable      : false,
      allowBlank    : false,
      displayField  : conf.select.displayField,
      valueField    : conf.select.valueField,
      mode          : 'remote',
      triggerAction : 'all',
      store         : new Ext.data.JsonStore({
        root        : 'entries',
        url         : conf.select.url,
        baseParams  : conf.select.params,
        fields      : [ conf.select.valueField, conf.select.displayField ]
      }),
      listeners     : {
        select: function (s, n, o) {
          params = conf.select.clazz.params || {};
          params['uuid'] = puuid = n.data.uuid;
          Ext.Ajax.request({
            url     : conf.select.clazz.url || conf.select.url || conf.url,
            success : function(d) {
              panel.remove(s);
              build_form(json_decode(d));
            },
            params  : params
          });
        }
      }
    });

    panel.add(combo);
    win.show();
  }
}


/*
 * IDnode grid
 */
tvheadend.idnode_grid = function(panel, conf)
{
  function build (d)
  {
    var columns = [];
    var filters = [];
    var fields  = [];
    var buttons = [];
    var saveBtn = null;
    var undoBtn = null;
    var addBtn  = null;
    var delBtn  = null;

    /* Load Class */
    d = json_decode(d);

    /* Process */
    for (i = 0; i < d.length; i++) {
      var type = 'string';
      var edit = null;
      if (d[i].type == 'int' || d[i].type == 'u16' || d[i].type == 'u32') {
        type = 'numeric';
        if (!d[i].rdonly)
          edit = new Ext.form.NumberField({
            // TODO: min/max
          })
      } else if (d[i].type == 'bool') {
        type = 'boolean';
        if (!d[i].rdonly)
          edit = new Ext.form.Checkbox({});
      } else if (d[i].type == 'str') {
        if (!d[i].rdonly)
          edit = new Ext.form.TextField({});
      }
      fields.push(d[i].id)
      columns.push({
        dataIndex: d[i].id,
        header   : d[i].caption,
        sortable : true,
        editor   : edit
      });
      filters.push({
        type      : type,
        dataIndex : d[i].id
      });
    }

    /* Filters */
    var filter = new Ext.ux.grid.GridFilters({
      encode  : true,
      local   : false,
      filters : filters
    });

    /* Store */
    var store  = new Ext.data.JsonStore({
      root          : 'entries',
      url           : conf.url,
      autoLoad      : true,
      id            : 'uuid',
      totalProperty : 'total',
      fields        : fields,
      baseParams : {
        op : 'list',
      }
    });

    /* Model */
    var model = new Ext.grid.ColumnModel({
      defaultSortable : true,
      columns         : columns
    });

    /* Selection */
    var select = new Ext.grid.RowSelectionModel({
      singleSelect    : false
    });

    /* Event handlers */
    store.on('update', function(s, r, o){
      d = (s.getModifiedRecords().length == 0);
      undoBtn.setDisabled(d);
      saveBtn.setDisabled(d);
    });
    select.on('selectionchange', function(s){
      if (delBtn)
        delBtn.setDisabled(s.getCount() == 0);
    });

    /* Top bar */
    saveBtn = new Ext.Toolbar.Button({
      tooltip     : 'Save pending changes (marked with red border)',
      iconCls     : 'save',
      text        : 'Save',
      disabled    : true,
      handler     : function(){}
    });
    buttons.push(saveBtn);
    undoBtn = new Ext.Toolbar.Button({
      tooltip     : 'Revert pending changes (marked with red border)',
      iconCls     : 'undo',
      text        : 'Undo',
      disabled    : true,
      handler     : function() {
        store.rejectChanges();
      }
    });
    buttons.push(undoBtn);
    buttons.push('-');
    if (conf.add) {
      addBtn  = new Ext.Toolbar.Button({
        tooltip     : 'Add a new entry',
        iconCls     : 'add',
        text        : 'Add',
        disabled    : false,
        handler     : function() {
          tvheadend.idnode_create(conf.add);
        }
      });
      buttons.push(addBtn);
    }
    if (conf.del) {
      delBtn  = new Ext.Toolbar.Button({
        tooltip     : 'Delete selected entries',
        iconCls     : 'remove',
        text        : 'Delete',
        disabled    : true,
        handler     : function(){}
      });
      buttons.push(delBtn);
    }
    buttons.push('->');
    if (conf.help) {
      buttons.push({  
        text    : 'Help',
        handler : conf.help
      });
    }

    /* Grid Panel */
    var grid   = new Ext.grid.EditorGridPanel({
      stripeRows    : true,
      title         : conf.title,
      store         : store,
      cm            : model,
      selModel      : select,
      plugins       : [
        filter
      ],
      viewConfig    : {
        forceFit : true
      },
      tbar          : buttons,
      bbar          : new Ext.PagingToolbar({
        store       : store,
        pageSize    : 50,
        displayInfo : true,
        displayMsg  :  conf.title + ' {0} - {1} of {2}',
        emptyMsg    : 'No ' + conf.title.toLowerCase() + ' to display'
      })
    });

    panel.add(grid);
  }

  Ext.Ajax.request({
    url     : conf.url,
    success : build,
    params  : {
      op: 'class'
    }
  });
}
