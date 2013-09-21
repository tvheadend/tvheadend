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
  fields     : [ 'key', 'val' ],
  id         : 'key',
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
        valueField   : 'key',
        displayField : 'val',
        clazz        : {
          url          : 'api/mpegts/network/mux_class'
        }
      },
      create   : {
        url    : 'api/mpegts/network/mux_create',
      }
    },
    del     : true,
    lcol    : [
      {
        header   : 'Play',
        renderer : function(v, o, r) {
          return "<a href='stream/mux/" + r.id + "'>Play</a>";
        }
      }
    ]
  });
}

tvheadend.services = function(panel)
{
  var mapButton = new Ext.Toolbar.Button({
    tooltip : 'Map services to channels',
    iconCls : '',
    text    : 'Map Services',
    callback: tvheadend.mapServices,
    disabled : false,
  });
  tvheadend.idnode_grid(panel, {
    url     : 'api/mpegts/service',
    comet   : 'service',
    titleS  : 'Service',
    titleP  : 'Services',
    add     : false,
    del     : false,
    tbar    : [ mapButton ],
    lcol    : [
      {
        header   : 'Play',
        renderer : function(v, o, r) {
          return "<a href='stream/service/" + r.id + "'>Play</a>";
        }
      }
    ]
  });
}

tvheadend.satconfs = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url      : 'api/linuxdvb/satconf',
    comet    : 'linuxdvb_satconf',
    titleS   : 'Satconf',
    titleP   : 'Satconfs',
    add      : {
      titleS : 'Satconf',
      url    : 'api/linuxdvb/satconf',
      create : {}
    },
    del      : true,
    edittree : true,
  });
}
