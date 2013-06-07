/*
 * IDnode stuff
 */

/*
 * DVB network
 */

tvheadend.networks = function(panel)
{
  var fields = [
    {
      type    : 'str',
      id      : 'name',
      caption : 'Network Name'
    },
    {
      type    : 'u16',
      id      : 'nid',
      caption : 'Network ID'
    },
    {
      type    : 'bool',
      id      : 'initscan',
      caption : 'Initial scan'
    },
    {
      type    : 'bool',
      id      : 'autodiscovery',
      caption : 'Auto-discovery'
    }
  ];

  tvheadend.idnode_grid(panel, {
    title    :  'Network',
    url      : 'api/mpegts/network',
    //fields  : fields,
    add       : {
      url    : 'api/mpegts/input',
      title  : 'Network',
      select : {
        caption      : 'Input',
        params       : { op: 'list', limit: -1 },
        displayField : 'fe_path',//displayname',
        valueField   : 'uuid',
        url          : 'api/mpegts/input',
        clazz        : {
          params  : { op: 'network_class' }
        }
      },
      create : {
        params : { op: 'network_create' }
      }
    },
    del     : true
  });
}

tvheadend.muxes = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url   : 'api/mpegts/mux',
    title : 'Muxes',
    add   : true,
    del   : true
  });
}

tvheadend.services = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url   : 'api/mpegts/service',
    title : 'Services',
    add   : false,
    del   : false
  });
}
