/*
 * IDnode stuff
 */

/*
 * DVB network
 */

tvheadend.networks = function(panel)
{
  tvheadend.idnode_grid(panel, {
    title   : 'Networks',
    url       : 'api/mpegts/network',
    add       : {
      url    : 'api/mpegts/input',
      title  : 'Network',
      select : {
        label        : 'Input',
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
