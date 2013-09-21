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

/*
 * Service mapping
 */
tvheadend.mapServices = function(t, e, store, select)
{
  var panel = null;
  var win   = null;

  /* Form fields */
  var availCheck = new Ext.form.Checkbox({
    name        : 'check_availability',
    fieldLabel  : 'Check availability',
    checked     : false
  });
  var ftaCheck   = new Ext.form.Checkbox({
    name        : 'encrypted',
    fieldLabel  : 'Include encrypted services',
    checked     : false,
    // TODO: make dependent on CSA config
  });
  var mergeCheck = new Ext.form.Checkbox({
    name        : 'merge_same_name',
    fieldLabel  : 'Merge same name',
    checked     : false
  });
  var provtagCheck = new Ext.form.Checkbox({
    name        : 'provider_tags',
    fieldLabel  : 'Create provider tags',
    checked     : false
  });
  // TODO: provider list
  items = [ availCheck, ftaCheck, mergeCheck, provtagCheck ];

  /* Form */
  var undoBtn = new Ext.Button({
    text    : 'Cancel',
    handler : function () {
      win.close();
    }
  });

  var saveBtn = new Ext.Button({  
    text    : 'Map',
    tooltip : 'Begin mapping',
    handler : function () {
      p = null;
      if (select) {
        var r = select.getSelections();
        if (r.length > 0) {
          var uuids = [];
          for (var i = 0; i < r.length; i++)
            uuids.push(r[i].id);
          p = { uuids: Ext.encode(uuids) };
        }
      }
      panel.getForm().submit({
        url         : 'api/service/mapper/start',
        waitMessage : 'Mapping services...',
        params      : p
      });
    }
  });

  panel = new Ext.FormPanel({
    frame       : true,
    border      : true,
    bodyStyle   : 'padding: 5px',
    labelAlign  : 'left',
    labelWidth  : 200,
    autoWidth   : true,
    autoHeight  : true,
    defaultType : 'textfield',
    buttonAlign : 'left',
    items       : items,
    buttons     : [ undoBtn, saveBtn ]
  });
   
  /* Create window */
  win = new Ext.Window({
    title       : 'Map services',
    layout      : 'fit',
    autoWidth   : true,
    autoHeight  : true,
    plain       : true,
    items       : panel
  });

  win.show();
}

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
    tooltip : 'Map services to channels',
    iconCls : '',
    text    : 'Map Services',
    handler : tvheadend.mapServices,
    disabled : false,
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
