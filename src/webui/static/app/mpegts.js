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
    tabIndex : 1,
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
    tabIndex : 2,
    hidemode : true,
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
    iconCls : 'clone',
    text    : 'Map All',
    callback: tvheadend.mapServices,
    disabled : false,
  });
  var selected = function (s)
  {
    if (s.getCount() > 0)
      mapButton.setText('Map Selected')
    else
      mapButton.setText('Map All')
  }
  tvheadend.idnode_grid(panel, {
    url      : 'api/mpegts/service',
    comet    : 'service',
    titleS   : 'Service',
    titleP   : 'Services',
    tabIndex : 3,
    hidemode : true,
    add      : false,
    del      : false,
    selected : selected,
    tbar     : [ mapButton ],
    lcol     : [
      {
        header   : 'Play',
        renderer : function(v, o, r) {
          return "<a href='stream/service/" + r.id + "'>Play</a>";
        }
      }
    ]
  });
}
