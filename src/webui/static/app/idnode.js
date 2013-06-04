/*
 * IDnode grid
 */
tvheadend.idnode_grid = function(panel, conf)
{
  function build (d)
  {
    if (d && d.responseText) {
      d = Ext.util.JSON.decode(d.responseText)
      d = d.entries;
    } else
      d = []

    /* Process object */
    var columns = [];
    var filters = [];
    var fields  = [];
    for (i = 0; i < d.length; i++) {
      var type = 'string';
      if (d[i].type == 'int' || d[i].type == 'u16' || d[i].type == 'u32')
        type = 'numeric';
      else if (d[i].type == 'bool')
        type = 'boolean';
      fields.push(d[i].id)
      columns.push({
        dataIndex: d[i].id,
        header   : d[i].caption,
        sortable : true
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

    /* Grid Panel */
    var grid   = new Ext.grid.EditorGridPanel({
      stripeRows    : true,
      title         : conf.title,
      store         : store,
      cm            : model,
      plugins       : [
        filter
      ],
      viewConfig    : {
        forceFit : true
      },
      bbar : new Ext.PagingToolbar({
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
