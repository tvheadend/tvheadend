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
    del     : true,
    sort    : {
      field : 'networkname',
      direction : 'ASC'
    }
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
        width    : 50,
        header   : 'Play',
        renderer : function(v, o, r) {
          return "<a href='stream/mux/" + r.id + "'>Play</a>";
        }
      }
    ],
    sort    : {
      field  : 'name',
      direction : 'ASC'
    }
  });
}

tvheadend.show_service_streams = function ( data ) {
  var i, j;
	var html = '';

  html += '<table style="font-size:8pt;font-family:mono;padding:2px"';
	html += '<tr>';
	html += '<th style="width:50px;font-weight:bold">Index</th>';
	html += '<th style="width:120px;font-weight:bold">PID</th>';
	html += '<th style="width:100px;font-weight:bold">Type</th>';
	html += '<th style="width:75px;font-weight:bold">Language</th>';
  html += '<th style="width:*;font-weight:bold">Details</th>';
	html += '</tr>';

  function hexstr ( d ) {
    return ('0000' + d.toString(16)).slice(-4);
  }
    
  function fixstr ( d ) {
    var r = d.toString();
    var l = r.length;
    var i;
    for (i = l; i < 5; i++) {
      r = '&nbsp;' + r;
    }
    return r;
  }

	for (i = 0; i < data.streams.length; i++) {
		var s = data.streams[i];
    var d = '&nbsp;';
    var p = '0x' + hexstr(s.pid) + '&nbsp;/&nbsp;' + fixstr(s.pid);

		html += '<tr>';
		html += '<td>' + (s.index > 0 ? s.index : '&nbsp;') + '</td>';
		html += '<td>' + p + '</td>';
		html += '<td>' + s.type + '</td>';
		html += '<td>' + (s.language || '&nbsp;') + '</td>'
    if (s.type == 'CA') {
      d = 'CAIDS: ';
      for (j = 0; j < s.caids.length; j++) {
        d += s.caids[j].caid + ', ';
      }
    }
		html += '<td>' + d + '</td>';
		html += '</tr>';
	}

	var win = new Ext.Window({
		title : 'Service details for ' + data.name,
		layout : 'fit',
		width : 600,
		height : 300,
		plain : true,
		bodyStyle : 'padding: 5px',
		html : html
	});
	win.show();
}

tvheadend.services = function(panel)
{
  var mapButton = new Ext.Toolbar.Button({
    tooltip   : 'Map services to channels',
    iconCls   : 'clone',
    text      : 'Map All',
    callback  : tvheadend.service_mapper,
    disabled  : false,
  });
  var selected = function (s)
  {
    if (s.getCount() > 0)
      mapButton.setText('Map Selected')
    else
      mapButton.setText('Map All')
  }
  var actions = new Ext.ux.grid.RowActions({
    header    : '',
    width     : 10,
    actions   : [ {
      iconCls : 'info',
      qtip    : 'Detailed stream info',
      cb      : function ( grid, rec, act, row, col ) {
        Ext.Ajax.request({
          url    : 'api/service/streams',
          params : {
            uuid : rec.id
          },
          success : function (r, o) {
            var d = Ext.util.JSON.decode(r.responseText);
            tvheadend.show_service_streams(d);
          }
        });
      }
    } ]
  });
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
        width    : 50,
        header   : 'Play',
        renderer : function(v, o, r) {
          return "<a href='stream/service/" + r.id + "'>Play</a>";
        }
      },
      actions
    ],
    plugins   : [ actions ],
    sort     : {
      field  : 'svcname',
      direction : 'ASC'
    }
  });
}

tvheadend.mux_sched = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url      : 'api/mpegts/mux_sched',
    comet    : 'mpegts_mux_sched',
    titleS   : 'Mux Scheduler',
    titleP   : 'Mux Schedulers',
    tabIndex : 4,
    hidemode : true,
    add      : {
      url    : 'api/mpegts/mux_sched',
      titleS : 'Mux Scheduler',
      create : {
        url          : 'api/mpegts/mux_sched/create'
      }
    },
    del      : true
  });
}
