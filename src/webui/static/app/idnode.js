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
 * Build enum data store
 */
tvheadend.idnode_enum_store = function(f)
{
  var store = null;
  switch (f.type) {
    case 'str':
      if (f.enum.length > 0 && f.enum[0] instanceof Object)
        store = new Ext.data.JsonStore({
          id      : 'key',
          fields  : [ 'key', 'val' ],
          data    : f.enum,
        });
      else
        store = f.enum;
      break;
    case 'int':
    case 'u32':
    case 'u16':
    case 'dbl':
      var data  = null;
      if (f.enum.length > 0 && f.enum[0] instanceof Object) {
        data = f.enum;
      } else {
        data = []
        for ( i = 0; i < f.enum.length; i++ )
          data.push({ key: i, val: f.enum[i]});
      }
      store = new Ext.data.JsonStore({
        id     : 'key',
        fields : [ 'key', 'val' ],
        data   : data
      });
      break;
  }
  return store;
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
        return new Ext.form.ComboBox({
          fieldLabel      : f.caption,
          name            : f.id,
          value           : f.value,
          disabled        : d,
          width           : 300,
          mode            : 'local',
          valueField      : 'key',
          displayField    : 'val',
          store           : tvheadend.idnode_enum_store(f),
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
      break;

    case 'bool':
      return new Ext.form.Checkbox({
        fieldLabel  : f.caption,
        name        : f.id,
        checked     : f.value,
        disabled    : d
      });
      break;

    case 'int':
    case 'u32':
    case 'u16':
    case 'dbl':
      if (f.enum) {
        return new Ext.form.ComboBox({
          fieldLabel      : f.caption,
          name            : f.id,
          value           : f.value,
          disabled        : d,
          width           : 300,
          mode            : 'local',
          valueField      : 'key',
          displayField    : 'val',
          store           : tvheadend.idnode_enum_store(f),
          typeAhead       : true,
          forceSelection  : true,
          triggerAction   : 'all',
          emptyText       :'Select ' + f.caption +' ...'
        });
      } else {
        return new Ext.form.NumberField({
          fieldLabel  : f.caption,
          name        : f.id,
          value       : f.value,
          disabled    : d,
          width       : 300
        });
      }
      break;

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
    text  : 'Save',
    handler     : function() {
      var node = panel.getForm().getFieldValues();
      node.uuid  = item.uuid;
      var params = {
        op    : 'save',
        nodes : Ext.util.JSON.encode([node])
      };
      Ext.Ajax.request({
        url      : 'api/idnode',
        params   : params,
        success : function(d) {
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
    width  : 600,
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
  var puuid  = null;
  var panel  = null;
  var pclass = null;

  /* Buttons */
  var saveBtn = new Ext.Button({
    tooltip     : 'Create new entry',
    text        : 'Create',
    hidden      : true,
    handler     : function(){
      params = conf.create.params || {}
      if (puuid)
        params['uuid'] = puuid;
      if (pclass)
        params['class'] = pclass
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
    var store = conf.select.store;
    if (!store) {
      store = new Ext.data.JsonStore({
        root        : 'entries',
        url         : conf.select.url || conf.url,
        baseParams  : conf.select.params,
        fields      : [ conf.select.valueField, conf.select.displayField ]
      });
    }
    var select = null;
    if (conf.select.propField) {
      select = function (s, n, o) {
        var r =  store.getAt(s.selectedIndex);
        if (r) {
          var d = r.get(conf.select.propField);
          if (d) {
            pclass = r.get(conf.select.valueField);
            win.setTitle('Add ' + s.lastSelectionText);
            panel.remove(s);
            build_form(d);
          }
        }
      }
    } else {
      select = function (s, n, o) {
        params = conf.select.clazz.params || {};
        params['uuid'] = puuid = n.data.uuid;
        Ext.Ajax.request({
          url     : conf.select.clazz.url || conf.select.url || conf.url,
          success : function(d) {
            panel.remove(s);
            d = json_decode(d);
            build_form(d.props);
          },
          params  : params
        });
      };
    }

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
      store         : store,
      listeners     : {
        select : select
      }
    });

    panel.add(combo);
    win.show();
  } else {
    Ext.Ajax.request({
      url     : conf.url,
      params  : conf.params,
      success : function(d) {
        d = json_decode(d);
        build_form(d.props);
        win.show();
      }
    });
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
      var f    = d[i];
      var type = 'string';
      var edit = null;
      var rend = null;
      if (f.type == 'separator') continue;
      if (!f.rdonly && !f.wronce)
        edit = tvheadend.idnode_editor_field(f);
      if (f.enum)
        rend = function(v) {
          var s = null;
          for (j = 0; j < d.length; j++) {
            if (d[j].id == this.dataIndex) {
              s = tvheadend.idnode_enum_store(d[j]);
              break;
            }
          }
          if (s && s.find) {
            var r = s.find('key', v);
            if (r != -1) {
              v = s.getAt(r).get('val');
            }
          }
          return v;
        }
      fields.push(f.id)
      columns.push({
        dataIndex: f.id,
        header   : f.caption,
        sortable : true,
        editor   : edit,
        renderer : rend,
      });
      filters.push({
        type      : type,
        dataIndex : f.id
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
      handler     : function(){
        var mr  = store.getModifiedRecords();
        var out = new Array();
        for (var x = 0; x < mr.length; x++) {
          v           = mr[x].getChanges();
          out[x]      = v;
          out[x].uuid = mr[x].id;
        }
        Ext.Ajax.request({
           url     : 'api/idnode',
           params  : {
             op    : 'save',
             nodes : Ext.encode(out)
           },
           success : function(d)
           {
           }
        });
      }
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
        var r = select.getSelected();
        if (r) {
          if (conf.edittree) {
            var p = tvheadend.idnode_tree({
              url     : 'api/idnode',
              params  : {
                op   : 'childs',
                root : r.id
              }
            });
            p.setSize(800,600);
            var w = new Ext.Window({
              title       : 'Edit ' + conf.titleS,
              layout      : 'fit',
              autoWidth   : true,
              autoHeight  : true,
              plain       : true,
              items       : p
            });
            w.show();
          } else {
            Ext.Ajax.request({
              url     : 'api/idnode',
              params  : {
                op: 'get',
                uuid: r.id
              },
              success : function(d)
              {
                d = json_decode(d);
                var p = tvheadend.idnode_editor(d[0], {});
                var w = new Ext.Window({
                  title       : 'Edit ' + conf.titleS,
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
    var auto   = new Ext.form.Checkbox({
      checked     : true,
      listeners   : {
        check : function ( s, c ) {
          if (c) store.reload();
        }
      }
    });
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
        emptyMsg    : 'No ' + conf.titleP.toLowerCase() + ' to display',
        items       : [ '-', 'Auto-refresh', auto ]
      })
    });
    panel.add(grid);

    /* Add comet listeners */
    var update = function(o) {
      if (auto.getValue())
        store.reload();
    };
    if (conf.comet)
      tvheadend.comet.on(conf.comet, update);
    tvheadend.comet.on('idnodeUpdated', update);
    tvheadend.comet.on('idnodeDeleted', update);
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
        build(json_decode(d).props);
      }
    });
  } else {
    build(conf.fields);
  }
}

tvheadend.idnode_tree = function (conf)
{
  var current = null;
  var params  = conf.params || {};
  params.op = 'childs';
  var loader = new Ext.tree.TreeLoader({
    dataUrl         : conf.url,
    baseParams      : params,
    preloadChildren : conf.preload,
    nodeParameter   : 'uuid'
  });

  var tree = new Ext.tree.TreePanel({
    loader  : loader,
    flex    : 1,
    border  : false,
    root    : new Ext.tree.AsyncTreeNode({
      id    : conf.root  || 'root',
      text  : conf.title || ''
    }),
    listeners : {
      click: function(n) {
        if(current)
          panel.remove(current);
        if(!n.isRoot)
          current = panel.add(new tvheadend.idnode_editor(n.attributes, {
            title       : 'Parameters',
            fixedHeight : true
          }));
        panel.doLayout();
      }
    }
  });

  // TODO: top-level reload
  tvheadend.comet.on('idnodeUpdated', function(o) {
    var n = tree.getNodeById(o.uuid);
    if (n) {
      if (o.text) n.setText(o.text);
      loader.load(n);
    }
  });


  var panel = new Ext.Panel({
    title          : conf.title || '',
    layout        : 'hbox',
    flex          : 1,
    padding        : 5,
    border        : false,
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
