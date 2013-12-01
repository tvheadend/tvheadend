/**
 * Channel tags
 */
tvheadend.channelTags = new Ext.data.JsonStore({
	autoLoad : true,
	root : 'entries',
	fields : [ 'identifier', 'name' ],
	id : 'identifier',
	url : 'channeltags',
	baseParams : {
		op : 'listTags'
	}
});

tvheadend.channelTags.setDefaultSort('name', 'ASC');

tvheadend.comet.on('channeltags', function(m) {
	if (m.reload != null) tvheadend.channelTags.reload();
});

/**
 * Channels
 */
tvheadend.channelrec = new Ext.data.Record.create(
	[ 'name', 'chid', 'epggrabsrc', 'tags', 'ch_icon', 'epg_pre_start',
		'epg_post_end', 'number' ]);

tvheadend.channels = new Ext.data.JsonStore({
	url 	  : 'api/channel/list',
	root 	  : 'entries',
	fields 	  : [ 'key', 'val' ],
	id 	  : 'key',
	autoLoad  : true,
  sortInfo : {
    field : 'val',
    direction : 'ASC'
  }
});

tvheadend.comet.on('channels', function(m) {
	if (m.reload != null) tvheadend.channels.reload();
});

tvheadend.channel_tab = function(panel)
{
  var mapButton = new Ext.Toolbar.Button({
    tooltip   : 'Map services to channels',
    iconCls   : '',
    text      : 'Map Services',
    handler   : tvheadend.service_mapper,
    disabled  : false,
  });

  tvheadend.idnode_grid(panel, {
    url     : 'api/channel',
    comet   : 'channel',
    titleS  : 'Channel',
    titleP  : 'Channels',
    tabIndex: 0,
    add     :  {
      url    : 'api/channel',
      create : {}
    },
    del     : true,
    tbar    : [ mapButton ],
    lcol    : [
      {
        header   : 'Play',
        renderer : function (v, o, r) {
          return "<a href='stream/channel/" + r.id + "'>Play</a>";
        }
      }
    ]
  });
}
