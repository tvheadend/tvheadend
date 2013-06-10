/*
 * IDnode stuff
 */

/*
 * DVB network
 */

tvheadend.networks = function(panel)
{
  tvheadend.idnode_grid(panel, {
    titleS   :  'Network',
    titleP   :  'Networks',
    url      : 'api/mpegts/network',
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
    titleS   : 'Mux',
    titleP   : 'Muxes',
    url      : 'api/mpegts/mux',
    add       : {
      title  : 'Mux',
      url    : 'api/mpegts/network',
      select : {
        caption      : 'Network',
        params       : { op: 'list', limit: -1 },
        displayField : 'networkname',
        valueField   : 'uuid',
        clazz        : {
          params  : { op: 'mux_class' }
        }
      },
      create : {
        params : { op: 'mux_create' }
      }
    },
    del     : true
  });
}

tvheadend.services = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url     : 'api/mpegts/service',
    titleS  : 'Service',
    titleP  : 'Services',
    add     : false,
    del     : false
  });
}
