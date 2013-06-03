/*
 * DVB Networks
 */

tvheadend.networks = function()
{
  // TODO: build this from mpegts_network_class?

  var model = new Ext.grid.ColumnModel({
    defaultSortable: true,
    columns        : [
      {
        header      : 'Name',
        dataIndex   : 'name',
        width       : 200,
        editor      : new Ext.form.TextField({
          allowBlank : true
        })
      },
      {
        header      : 'Network ID',
        dataIndex   : 'nid',
        width       : 100,
        editor      : new Ext.form.NumberField({
          minValue : 0,
          maxValue : 65536
        })
      },
      {
        header      : 'Auto Discovery',
        dataIndex   : 'autodiscovery',
        width       : 50,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : 'Auto Discovery',
        dataIndex   : 'skipinitscan',
        width       : 50,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : '# Muxes',
        dataIndex   : 'num_mux',
        width       : 100,
      },
      {
        header      : '# Services',
        dataIndex   : 'num_svc',
        width       : 100,
      },
    ]
  });

  var store = new Ext.data.JsonStore({
    root      : 'entries',
    url       : 'api/mpegts/networks',
    autoLoad  : true,
    id        : 'uuid',
    fields    : [
      'uuid', 'name', 'nid', 'autodiscovery', 'skipinitscan', 'num_mux', 'num_svc'
    ],
    baseParams : {
      op : 'list',
    }
  });

  var grid = new Ext.grid.EditorGridPanel({
    stripeRows    : true,
    title         : 'Networks',
    store         : store,
    cm            : model,
    viewConfig    : {
      forceFit : true
    },
  });
  
  return grid;
}

/*
 * DVB Muxes
 */

tvheadend.muxes = function()
{
  // TODO: build this from mpegts_network_class?

  var model = new Ext.grid.ColumnModel({
    defaultSortable: true,
    columns        : [
      {
        header      : 'Enabled',
        dataIndex   : 'enabled',
        width       : 50,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : 'Name',
        dataIndex   : 'name',
        width       : 100,
      },
      {
        header      : 'ONID',
        dataIndex   : 'onid',
        width       : 50,
      },
      {
        header      : 'TSID',
        dataIndex   : 'tsid',
        width       : 50,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : 'CRID Authority',
        dataIndex   : 'crid_auth',
        width       : 100,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : 'Initial Scan',
        dataIndex   : 'initscan',
        width       : 50,
      },
      {
        header      : '# Services',
        dataIndex   : 'num_svc',
        width       : 50,
      },
    ]
  });

  var store = new Ext.data.JsonStore({
    root      : 'entries',
    url       : 'api/mpegts/muxes',
    autoLoad  : true,
    id        : 'uuid',
    fields    : [
      'uuid', 'network', 'name', 'onid', 'tsid', 'initscan', 'crid_auth', 'num_svc'
    ],
    baseParams : {
      op : 'list',
    }
  });

  var grid = new Ext.grid.EditorGridPanel({
    stripeRows    : true,
    title         : 'Muxes',
    store         : store,
    cm            : model,
    viewConfig    : {
      forceFit : true
    },
  });
  
  return grid;
}

/*
 * DVB Services
 */

tvheadend.services = function()
{
  // TODO: build this from mpegts_network_class?

  var filter = new Ext.ux.grid.GridFilters({
    encode  : true,
    local   : false,
    filters : [
      { type: 'string', dataIndex: 'name' },
      { type: 'numeric', dataIndex: 'sid' }
    ]
  });

  var model = new Ext.grid.ColumnModel({
    defaultSortable: true,
    columns        : [
      {
        header      : 'Enabled',
        dataIndex   : 'enabled',
        width       : 50,
        editor      : new Ext.form.Checkbox({})
      },
      {
        header      : 'Name',
        dataIndex   : 'name',
        width       : 100,
        filterable  : true,
      },
      {
        header      : 'Mux',
        dataIndex   : 'mux',
        width       : 100,
      },
      {
        header      : 'SID',
        dataIndex   : 'sid',
        width       : 50,
      },
      {
        header      : 'PMT PID',
        dataIndex   : 'pmt',
        width       : 50,
      },
      {
        header      : 'Type',
        dataIndex   : 'type',
        width       : 50,
      },
      {
        header      : 'Provider',
        dataIndex   : 'provider',
        width       : 100,
      },
      {
        header      : 'CRID Authority',
        dataIndex   : 'crid_auth',
        width       : 100,
      },
      {
        header      : 'Charset',
        dataIndex   : 'charset',
        width       : 100,
      },
    ]
  });

  var store = new Ext.data.JsonStore({
    root      : 'entries',
    url       : 'api/mpegts/services',
    autoLoad  : true,
    id        : 'uuid',
    totalProperty: 'total',
    fields    : [
      'uuid', 'mux', 'name', 'sid', 'pmt', 'type', 'provider', 'crid_auth', 'charset'
    ],
    baseParams : {
      op : 'list',
    }
  });

  var grid = new Ext.grid.EditorGridPanel({
    stripeRows    : true,
    title         : 'Services',
    store         : store,
    cm            : model,
    plugins       : [
      filter
    ],
    viewConfig    : {
      forceFit : true
    },
    bbar : new Ext.PagingToolbar({
      store:    store,
      pageSize: 50,
      displayInfo: true,
      displayMsg:  'Services {0} - {1} of {2}',
      emptyMsg: 'No services to display'
    })
  });
  
  return grid;
}
