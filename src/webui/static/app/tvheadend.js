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

/**
 * Displays a mediaplayer using VLC plugin
 */
tvheadend.VLC = function(url) {
	
	var vlc = Ext.get(document.createElement('embed'));
	vlc.set({
		type : 'application/x-vlc-plugin',
		pluginspage : 'http://www.videolan.org',
		version : 'VideoLAN.VLCPlugin.2',
		width : '100%',
		height : '100%',
		autoplay : 'no'
	});

	var vlcPanel = new Ext.Panel({
	    border : false,
	    layout : 'fit',
	    bodyStyle : 'background: transparent;',
	    contentEl: vlc
	});

	var missingPlugin = Ext.get(document.createElement('div'));
	var missingPluginPanel = new Ext.Panel({
	    border : false,
	    layout : 'fit',
	    bodyStyle : 'background: transparent;',
	    contentEl : missingPlugin
	});

	var selectChannel = new Ext.form.ComboBox({
		loadingText : 'Loading...',
		width : 200,
		displayField : 'name',
		store : tvheadend.channels,
		mode : 'local',
		editable : false,
		triggerAction : 'all',
		emptyText : 'Select channel...'
	});
	
	selectChannel.on('select', function(c, r) {
		var streamurl = 'stream/channelid/' + r.data.chid;
		var playlisturl = 'playlist/channelid/' + r.data.chid;

		if (!vlc.dom.playlist || vlc.dom.playlist == 'undefined') {
			var html = '<p>Embedded player could not be started. <br> You are probably missing VLC Mozilla plugin for your browser.</p>';
			html += '<p><a href="' + playlisturl	+ '">M3U Playlist</a></p>';
			html += '<p><a href="' + streamurl + '">Direct URL</a></p>';
			missingPlugin.dom.innerHTML = html;
			missingPluginPanel.show();
			vlcPanel.hide();
		}
		else {
			vlc.dom.playlist.stop();
			vlc.dom.playlist.items.clear();
			vlc.dom.playlist.add(streamurl);
			vlc.dom.playlist.playItem(0);
			vlc.dom.audio.volume = slider.getValue();
			missingPluginPanel.hide();
			vlcPanel.show();
		}
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
		if (vlc.dom.playlist && vlc.dom.playlist.isPlaying) {
			vlc.dom.audio.volume = slider.getValue();
			sliderLabel.setText(vlc.dom.audio.volume + '%');
		}
		else {
			sliderLabel.setText(slider.getValue() + '%');
		}
	});

	var win = new Ext.Window({
		title : 'VLC Player',
		layout : 'fit',
		width : 507 + 14,
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
					if (vlc.dom.playlist && vlc.dom.playlist.items.count
						&& !vlc.dom.playlist.isPlaying) {
						vlc.dom.playlist.play();
					}
				}
			},
			{
				iconCls : 'control_pause',
				tooltip : 'Pause',
				handler : function() {
					if (vlc.dom.playlist && vlc.dom.playlist.items.count) {
						vlc.dom.playlist.togglePause();
					}
				}
			},
			{
				iconCls : 'control_stop',
				tooltip : 'Stop',
				handler : function() {
					if (vlc.dom.playlist) {
						vlc.dom.playlist.stop();
					}
				}
			},
			'-',
			{
				iconCls : 'control_fullscreen',
				tooltip : 'Fullscreen',
				handler : function() {
					if (vlc.dom.playlist && vlc.dom.playlist.isPlaying
						&& (vlc.dom.VersionInfo.substr(0, 3) != '1.1')) {
						vlc.dom.video.toggleFullscreen();
					}
					else if (vlc.dom.VersionInfo.substr(0, 3) == '1.1') {
						alert('Fullscreen mode is broken in VLC 1.1.x');
					}
				}
			}, '-', {
				iconCls : 'control_volume',
				tooltip : 'Volume',
				disabled : true
			}, ],
		items : [ vlcPanel, missingPluginPanel ]
	});

	win.on('beforeShow', function() {
		win.getTopToolbar().add(slider);
		win.getTopToolbar().add(new Ext.Toolbar.Spacer());
		win.getTopToolbar().add(new Ext.Toolbar.Spacer());
		win.getTopToolbar().add(new Ext.Toolbar.Spacer());
		win.getTopToolbar().add(sliderLabel);

		// check if vlc plugin wasn't initialised correctly
		if (!vlc.dom.playlist || (vlc.dom.playlist == 'undefined')) {
			vlc.dom.style.display = 'none';
			var html = '<p>Embedded player could not be started. <br> You are probably missing VLC Mozilla plugin for your browser.</p>';

			if (url) {
				var channelid = url.substr(url.lastIndexOf('/'));
				var streamurl = 'stream/channelid/' + channelid;
				var playlisturl = 'playlist/channelid/' + channelid;
				html += '<p><a href="' + playlisturl	+ '">M3U Playlist</a></p>';
				html += '<p><a href="' + streamurl + '">Direct URL</a></p>';
			}
			missingPlugin.dom.innerHTML = html;
			vlcPanel.hide();
		}
		else {
			// check if the window was opened with an url-parameter
			if (url) {
				vlc.dom.playlist.items.clear();
				vlc.dom.playlist.add(url);
				vlc.dom.playlist.playItem(0);

				//enable yadif2x deinterlacer for vlc > 1.1
				var point1 = vlc.dom.VersionInfo.indexOf('.');
				var point2 = vlc.dom.VersionInfo.indexOf('.', point1 + 1);
				var majVersion = vlc.dom.VersionInfo.substring(0, point1);
				var minVersion = vlc.dom.VersionInfo.substring(point1 + 1, point2);
				if ((majVersion >= 1) && (minVersion >= 1)) 
					vlc.dom.video.deinterlace.enable("yadif2x");
			}
			missingPluginPanel.hide();
		}
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

	if (o.dvr == true && tvheadend.dvrpanel == null) {
		tvheadend.dvrpanel = new tvheadend.dvr;
		tvheadend.rootTabPanel.add(tvheadend.dvrpanel);
	}

	if (o.admin == true && tvheadend.confpanel == null) {
		tvheadend.confpanel = new Ext.TabPanel({
			activeTab : 0,
			autoScroll : true,
			title : 'Configuration',
			iconCls : 'wrench',
			items : [ new tvheadend.miscconf, new tvheadend.chconf,
				new tvheadend.epggrab, new tvheadend.cteditor,
				new tvheadend.dvrsettings, new tvheadend.tvadapters,
				new tvheadend.iptv, new tvheadend.acleditor,
				new tvheadend.cwceditor, new tvheadend.capmteditor,
				new tvheadend.ccweditor]
		});
		tvheadend.rootTabPanel.add(tvheadend.confpanel);
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

