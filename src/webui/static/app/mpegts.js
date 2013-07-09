/*
 * DVB network
 */

tvheadend.network_classes = new Ext.data.JsonStore({
  autoLoad	: true,
  root    	: 'entries',
  fields	: [ 'class', 'caption', 'props' ],
  id		: 'class',
  url		: 'api/mpegts/network',
  baseParams	: {
    op: 'class_list'
  }
});
tvheadend.comet.on('mpegts_network', function() {
  tvheadend.network_classes.reload();
});

tvheadend.networks = function(panel)
{
  tvheadend.idnode_grid(panel, {
    titleS   : 'Network',
    titleP   : 'Networks',
    url      : 'api/mpegts/network',
    comet    : 'mpegts_network',
    add      : {
      url    : 'api/mpegts/network',
      title  : 'Network',
      select : {
        caption      : 'Type',
	store        : tvheadend.network_classes,
        displayField : 'caption',
        valueField   : 'class',
        propField    : 'props',
      },
      create : {
        params : { op: 'create' }
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
    comet    : 'mpegts_mux',
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
    comet   : 'service',
    add     : false,
    del     : false
  });
}

tvheadend.satconfs = function(panel)
{
  tvheadend.idnode_grid(panel, {
    titleS   : 'Satconf',
    titleP   : 'Satconfs',
    url      : 'api/linuxdvb/satconf',
    comet    : 'linuxdvb_satconf',
    add      : {
      title  : 'Satconf',
      url    : 'api/linuxdvb/satconf',
      create : {
        params : { op: 'create' }
      },
      params : {
        op: 'class'
      }
    },
    del      : true,
    edittree : true,
  });
}
