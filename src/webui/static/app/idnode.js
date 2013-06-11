json_decode = function(d)
{
  if (d && d.responseText) {
    d = Ext.util.JSON.decode(d.responseText)
    if (d.entries)
      d = d.entries;
  } else {
    d = []
  }
  return d;
}

/*
 * Field editor
 */
tvheadend.idnode_editor_field = function(f, create)
{
  var d = f.rdonly || false;
  if (f.wronly && !create) d = false;

  switch(f.type) {
    case 'str':
      if (f.enum) {
        var store = f.enum;
        if (f.enum.length > 0 && f.enum[0] instanceof Object)
          store = new Ext.data.JsonStore({
            id          : 'key',
            fields      : [ 'key', 'val' ],
            data	: f.enum,
          });
        return new Ext.form.ComboBox({
          fieldLabel      : f.caption,
          name            : f.id,
          value           : f.value,
          disabled        : d,
          width           : 300,
          mode            : 'local',
          valueField      : 'key',
          displayField    : 'val',
          store           : store,
          typeAhead       : true,
          forceSelection  : true,
          triggerAction   : 'all',
          emptyText       :'Select ' + f.caption +' ...'
        });
      } else {
        return new Ext.form.TextField({
          fieldLabel  : f.caption,
          name        : f.id,
          value       : f.value,
          disabled    : d,
          width       : 300
        });
      }

    case 'bool':
      return new Ext.form.Checkbox({
        fieldLabel  : f.caption,
        name        : f.id,
        checked     : f.value,
        disabled    : d
      });

    case 'int':
    case 'u32':
    case 'u16':
      return new Ext.form.NumberField({
        fieldLabel  : f.caption,
        name        : f.id,
        value       : f.value,
        disabled    : d,
        width       : 300
      });

    /*
    case 'separator':
      return new Ext.form.LabelField({
        fieldLabel  : f.caption
      });
*/
  }
  return null;
}

/*
 * ID node editor panel
 */
tvheadend.idnode_editor = function(item, conf)
{
  var panel  = null;
  var fields = []

  for (var idx in item.params) {
    var f = tvheadend.idnode_editor_field(item.params[idx], true);
    if (f)
      fields.push(f);
  }

  /* Buttons */
  var saveBtn = new Ext.Button({
    text	: 'Save',
    handler     : function() {
      var params = {
        uuid: item.id,
        op  : 'save',
        conf: Ext.util.JSON.encode(panel.getForm().getFieldValues())
      };
      Ext.Ajax.request({
        url    	: 'api/idnode',
        params 	: params,
        success : function(d) {
          // TODO
        }
      });
    }
  });

  panel = new Ext.FormPanel({
    title       : conf.title || null,
    frame       : true,
    border      : true,
    bodyStyle   : 'padding: 5px',
    labelAlign  : 'left',
    labelWidth  : 200,
    autoWidth   : true,
    autoHeight  : !conf.fixedHeight,
    width	: 600,
    //defaults: {width: 330},
    defaultType : 'textfield',
    buttonAlign : 'left',
    items       : fields,
    buttons     : [ saveBtn ]
  });
  return panel;
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
      params['conf'] = Ext.util.JSON.encode(panel.getForm().getFieldValues());
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
      var f = tvheadend.idnode_editor_field(d[i]);
      if (f)
        panel.add(f);
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
        url         : conf.select.url || conf.url,
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
    var editBtn = null;

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
      remoteSort    : true,
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
      editBtn.setDisabled(s.getCount() != 1);
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
    buttons.push('-');
    editBtn = new Ext.Toolbar.Button({
      tooltip     : 'Edit selected entry',
      iconCls     : 'edit',
      text        : 'Edit',
      disabled    : true,
      handler     : function() {
        r = select.getSelected();
        if (r) {
          Ext.Ajax.request({
            url     : 'api/idnode',
            params  : {
              op: 'get',
              uuid: r.id
            },
            success : function(d)
            {
              d = json_decode(d);
              p = tvheadend.idnode_editor(d[0], {});
              w = new Ext.Window({
                title       : 'Add ' + conf.titleS,
                layout      : 'fit',
                autoWidth   : true,
                autoHeight  : true,
                plain       : true,
                items       : p
              });
              w.show();
            }
          });
        }
      }
    });
    buttons.push(editBtn);
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
      title         : conf.titleP,
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
        displayMsg  :  conf.titleP + ' {0} - {1} of {2}',
        emptyMsg    : 'No ' + conf.titleP.toLowerCase() + ' to display'
      })
    });

    panel.add(grid);
  }

  /* Request data */
  if (!conf.fields) {
    Ext.Ajax.request({
      url     : conf.url,
      params  : {
        op: 'class'
      },
      success : function(d)
      {
        build(json_decode(d));
      }
    });
  } else {
    build(conf.fields);
  }
}

tvheadend.idnode_tree = function (conf)
{
  var current = null;

  var loader = new Ext.tree.TreeLoader({
    dataUrl	: conf.url
  });

  var tree = new Ext.tree.TreePanel({
    loader	  : loader,
    flex	    : 1,
    border  	: false,
    root 	    : new Ext.tree.AsyncTreeNode({
      id 	  : 'root',
      text	: conf.title
    }),
    listeners : {
      click: function(n) {
        if(current)
          panel.remove(current);
        if(!n.isRoot)
          current = panel.add(new tvheadend.idnode_editor(n.attributes, {title: 'Parameters', fixedHeight: true}));
        panel.doLayout();
      }
    }
  });

  tvheadend.comet.on('idnodeNameChanged', function(o) {
    var n = tree.getNodeById(o.id);
    if(n) {
      n.setText(o.text);
    }
  });

  tvheadend.comet.on('idnodeParamsChanged', function(o) {
    var n = tree.getNodeById(o.id);
    if(n) {
      n.attributes.params = o.params;
   }
  });


  var panel = new Ext.Panel({
    title	        : conf.title,
    layout		    : 'hbox',
    flex		      : 1,
    padding		    : 5,
    border		    : false,
    layoutConfig  : {
      align : 'stretch'
    },
    items: [ tree ]
  });


  tree.on('render', function() {
    tree.getRootNode().expand();
  });

  return panel;
}
