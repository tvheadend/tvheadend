tvheadend.accessupdate = null;
tvheadend.capabilties  = null;
tvheadend.conf_chepg   = null;
tvheadend.conf_dvbin   = null;
tvheadend.conf_tsdvr   = null;
tvheadend.conf_csa     = null;

/* State Provider */
Ext.state.Manager.setProvider(new Ext.state.CookieProvider({
  // 7 days from now
  expires : new Date(new Date().getTime()+(1000*60*60*24*7)),
}));

/**
 * Displays a help popup window
 */
tvheadend.help = function(title, pagename) {
	Ext.Ajax.request({
		url : 'docs/' + pagename,
		success : function(result, request) {

			var content = new Ext.Panel({
				autoScroll : true,
				border : false,
				layout : 'fit',
				html : result.responseText
			});

			var win = new Ext.Window({
				title : 'Help for ' + title,
				layout : 'fit',
				width : 900,
				height : 400,
				constrainHeader : true,
				items : [ content ]
			});
			win.show();

		}
	});
}

/*
 * General capabilities
 */
Ext.Ajax.request({
  url: 'capabilities',
  success: function(d)
  {
    if (d && d.responseText)
      tvheadend.capabilities = Ext.util.JSON.decode(d.responseText);
    if (tvheadend.capabilities && tvheadend.accessupdate)
      accessUpdate(tvheadend.accessUpdate);
    
  }
});

/**
 * Displays a mediaplayer using the html5 video element
 */
tvheadend.VideoPlayer = function(url) {
	
    var videoPlayer = new tv.ui.VideoPlayer({
	params: {
	    resolution: 384
	}
    });

    var selectChannel = new Ext.form.ComboBox({
	loadingText : 'Loading...',
	width : 200,
	displayField : 'val',
	store : tvheadend.channels,
	mode : 'local',
	editable : true,
	triggerAction : 'all',
	emptyText : 'Select channel...'
    });
	
    selectChannel.on('select', function(c, r) {
	videoPlayer.zapTo(r.id);
    });

    var slider = new Ext.Slider({
	width : 135,
	height : 20,
	value : 90,
	increment : 1,
	minValue : 0,
	maxValue : 100
    });

    var sliderLabel = new Ext.form.Label();
    sliderLabel.setText("90%");
    slider.addListener('change', function() {
	videoPlayer.setVolume(slider.getValue());
	sliderLabel.setText(videoPlayer.getVolume() + '%');
    });

    var selectResolution = new Ext.form.ComboBox({
        width: 150,
        displayField:'name',
        valueField: 'res',
        value: 384,
        mode: 'local',
        editable: false,
        triggerAction: 'all',
        emptyText: 'Select resolution...',
        store: new Ext.data.SimpleStore({
            fields: ['res','name'],
            id: 0,
            data: [
                ['288','288p'],
                ['384','384p'],
                ['480','480p'],
                ['576','576p']
            ]
        })
    });

    selectResolution.on('select', function(c, r) {
	videoPlayer.setResolution(r.data.res);
	if(videoPlayer.isIdle())
	    return;
	
	var index = selectChannel.selectedIndex;
	if(index < 0)
	    return;
	
	var ch = selectChannel.getStore().getAt(index);
	videoPlayer.zapTo(ch.id);
    });

    var win = new Ext.Window({
	title : 'Live TV Player',
	layout : 'fit',
	width : 682 + 14,
	height : 384 + 56,
	constrainHeader : true,
	iconCls : 'eye',
	resizable : true,
	tbar : [
	    selectChannel,
	    '-',
	    {
		iconCls : 'control_play',
		tooltip : 'Play',
		handler : function() {
		    if(!videoPlayer.isIdle()) { //probobly paused
			videoPlayer.play();
			return;
		    }

		    var index = selectChannel.selectedIndex;
		    if(index < 0)
			return;
	
		    var ch = selectChannel.getStore().getAt(index);
		    videoPlayer.zapTo(ch.id);
		}
	    },
	    {
		iconCls : 'control_pause',
		tooltip : 'Pause',
		handler : function() {
		    videoPlayer.pause();
		}
	    },
	    {
		iconCls : 'control_stop',
		tooltip : 'Stop',
		handler : function() {
		    videoPlayer.stop();
		}
	    },
	    '-',
	    {
		iconCls : 'control_fullscreen',
		tooltip : 'Fullscreen',
		handler : function() {
		    videoPlayer.fullscreen();
		}
	    }, 
	    '-', 
	    selectResolution,
	    '-',
	    {
		iconCls : 'control_volume',
		tooltip : 'Volume',
		disabled : true
	    }, ],
	items : [videoPlayer]
    });
    
    win.on('beforeShow', function() {
	win.getTopToolbar().add(slider);
	win.getTopToolbar().add(new Ext.Toolbar.Spacer());
	win.getTopToolbar().add(new Ext.Toolbar.Spacer());
	win.getTopToolbar().add(new Ext.Toolbar.Spacer());
	win.getTopToolbar().add(sliderLabel);
    });
    
    win.on('close', function() {
	videoPlayer.stop();
    });

    win.show();
};

/**
 * This function creates top level tabs based on access so users without 
 * access to subsystems won't see them.
 *
 * Obviosuly, access is verified in the server too.
 */
function accessUpdate(o) {
  tvheadend.accessUpdate = o;
  if (!tvheadend.capabilities)
    return;

  if (o.dvr == true && tvheadend.dvrpanel == null) {
    tvheadend.dvrpanel = new tvheadend.dvr;
    tvheadend.rootTabPanel.add(tvheadend.dvrpanel);
  }

  if (o.admin == true && tvheadend.confpanel == null) {
    var tabs1 = [
      new tvheadend.miscconf,
      new tvheadend.acleditor
    ]
    var tabs2;

    /* DVB inputs */
    tabs2 = [];
    if (tvheadend.capabilities.indexOf('linuxdvb') != -1 ||
        tvheadend.capabilities.indexOf('v4l')      != -1) {
      tabs2.push(new tvheadend.tvadapters);
    }
/*
    tabs2.push(new tvheadend.iptv);
*/
    tvheadend.conf_dvbin = new Ext.TabPanel({
      activeTab: 0,
      autoScroll: true,
      title: 'DVB Inputs',
      iconCls: 'hardware',
      items : tabs2
    });
    tvheadend.networks(tvheadend.conf_dvbin);
    tvheadend.muxes(tvheadend.conf_dvbin);
    tvheadend.services(tvheadend.conf_dvbin);
    tabs1.push(tvheadend.conf_dvbin);

    /* Channel / EPG */
    tvheadend.conf_chepg = new Ext.TabPanel({
      activeTab: 0,
      autoScroll: true,
      title : 'Channel / EPG',
      iconCls : 'television',
      items : [
        new tvheadend.cteditor,
        new tvheadend.epggrab
      ]
    });
    tvheadend.channel_tab(tvheadend.conf_chepg);
    tabs1.push(tvheadend.conf_chepg);

    /* DVR / Timeshift */
    tabs2 = [ new tvheadend.dvrsettings ];
    if (tvheadend.capabilities.indexOf('timeshift') != -1) {
      tabs2.push(new tvheadend.timeshift)
    }
    tvheadend.conf_tsdvr = new Ext.TabPanel({
      activeTab: 0,
      autoScroll: true,
      title: 'Recording',
      iconCls: 'drive',
      items : tabs2
    });
    tabs1.push(tvheadend.conf_tsdvr);

    /* CSA */
    if (tvheadend.capabilities.indexOf('cwc')      != -1) {
      tvheadend.conf_csa = new Ext.TabPanel({
        activeTab: 0,
        autoScroll: true,
        title: 'CSA',
        iconCls: 'key',
        items: [
          new tvheadend.cwceditor,
          new tvheadend.capmteditor
        ]
      });
      tabs1.push(tvheadend.conf_csa);
    }

    /* Debug */
    tabs1.push(new tvheadend.tvhlog);

    tvheadend.confpanel = new Ext.TabPanel({
      activeTab : 0,
      autoScroll : true,
      title : 'Configuration',
      iconCls : 'wrench',
      items : tabs1
    });

    tvheadend.rootTabPanel.add(tvheadend.confpanel);
    tvheadend.confpanel.doLayout();
  }

  if (o.admin == true && tvheadend.statuspanel == null) {
    tvheadend.statuspanel = new tvheadend.status;
    tvheadend.rootTabPanel.add(tvheadend.statuspanel);
  }

  if (tvheadend.aboutPanel == null) {
    tvheadend.aboutPanel = new Ext.Panel({
      border : false,
      layout : 'fit',
      title : 'About',
      iconCls : 'info',
      autoLoad : 'about.html'
    });
    tvheadend.rootTabPanel.add(tvheadend.aboutPanel);
  }

  tvheadend.rootTabPanel.doLayout();
}

/**
 *
 */
function setServerIpPort(o) {
	tvheadend.serverIp = o.ip;
	tvheadend.serverPort = o.port;
}

function makeRTSPprefix() {
	return 'rtsp://' + tvheadend.serverIp + ':' + tvheadend.serverPort + '/';
}

/**
 *
 */
tvheadend.log = function(msg, style) {
	s = style ? '<div style="' + style + '">' : '<div>'

	sl = Ext.get('systemlog');
	e = Ext.DomHelper.append(sl, s + '<pre>' + msg + '</pre></div>');
	e.scrollIntoView('systemlog');
}

/**
 *
 */
//create application
tvheadend.app = function() {

	// public space
	return {

		// public methods
		init : function() {			
			var header = new Ext.Panel({
				split: true,
				region: 'north',
				height : 45,
				boxMaxHeight : 45,
				boxMinHeight : 45,
				border: false,
        hidden: true,
				html: '<div id="header"><h1>Tvheadend Web-Panel</h1></div>'
			});
			
			tvheadend.rootTabPanel = new Ext.TabPanel({
				region : 'center',
				activeTab : 0,
				items : [ new tvheadend.epg ]
			});

			var viewport = new Ext.Viewport({
				layout : 'border',
				items : [{
					region : 'south',
					contentEl : 'systemlog',
					split : true,
					autoScroll : true,
					height : 150,
					minSize : 100,
					maxSize : 400,
					collapsible : true,
					collapsed : true,
					title : 'System log',
					margins : '0 0 0 0',
					tools : [ {
						id : 'gear',
						qtip : 'Enable debug output',
						handler : function(event, toolEl, panel) {
							Ext.Ajax.request({
								url : 'comet/debug',
								params : {
									boxid : tvheadend.boxid
								}
							});
						}
					} ]
				}, tvheadend.rootTabPanel, header ]
			});

			tvheadend.comet.on('accessUpdate', accessUpdate);

			tvheadend.comet.on('setServerIpPort', setServerIpPort);

			tvheadend.comet.on('logmessage', function(m) {
				tvheadend.log(m.logtxt);
			});

			new tvheadend.cometPoller;

			Ext.QuickTips.init();
		}

	};
}(); // end of app

