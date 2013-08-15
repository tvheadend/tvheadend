/*
 * DVB network
 */

tvheadend.network_builders = new Ext.data.JsonStore({
  url		    : 'api/mpegts/network/builders',
  root    	: 'entries',
  fields	  : [ 'class', 'caption', 'props' ],
  id		    : 'class',
  autoLoad	: true,
});

tvheadend.network_list = new Ext.data.JsonStore({
  url        : 'api/idnode/load',
  baseParams : { class : 'mpegts_network', enum: 1 },
  root       : 'entries',
  fields     : [ 'uuid', 'title' ],
  id         : 'uuid',
  autoLoad   : true,
});

tvheadend.comet.on('mpegts_network', function() {
  // TODO: Might be a bit excessive
  tvheadend.network_builders.reload();
  tvheadend.network_list.reload();
});

tvheadend.networks = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url      : 'api/mpegts/network',
    comet    : 'mpegts_network',
    titleS   : 'Network',
    titleP   : 'Networks',
    add      : {
      titleS : 'Network',
      select : {
        label        : 'Type',
	      store        : tvheadend.network_builders,
        displayField : 'caption',
        valueField   : 'class',
        propField    : 'props',
      },
      create : {
        url          : 'api/mpegts/network/create'
      }
    },
    del     : true
  });
}

tvheadend.muxes = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url      : 'api/mpegts/mux',
    comet    : 'mpegts_mux',
    titleS   : 'Mux',
    titleP   : 'Muxes',
    add      : {
      titleS   : 'Mux',
      select   : {
        label        : 'Network',
        store        : tvheadend.network_list,
        displayField : 'title',
        valueField   : 'uuid',
        clazz        : {
          url          : 'api/mpegts/network/mux_class'
        }
      },
      create   : {
        url    : 'api/mpegts/network/mux_create',
      }
    },
    del     : true
  });
}

tvheadend.services = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url     : 'api/mpegts/service',
    comet   : 'service',
    titleS  : 'Service',
    titleP  : 'Services',
    add     : false,
    del     : false
  });
}

tvheadend.satconfs = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url      : 'api/linuxdvb/satconf',
    comet    : 'linuxdvb_satconf',
    titleS   : 'Satconf',
    titleP   : 'Satconfs',
    add      : {},
    del      : true,
    edittree : true,
  });
}
