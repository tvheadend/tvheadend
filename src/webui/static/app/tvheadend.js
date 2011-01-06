
/**
 * Displays a help popup window
 */
tvheadend.help = function(title, pagename) {
    Ext.Ajax.request({
	url: 'docs/' + pagename,
	success: function(result, request) { 

	    var content = new Ext.Panel({
		autoScroll:true,
		border: false,
		layout:'fit', 
		html: result.responseText
	    });

	    var win = new Ext.Window({
		title: 'Help for ' + title,
		layout: 'fit',
		width: 900,
		height: 400,
		constrainHeader: true,
		items: [content]
	    });    
	    win.show();

	}});
}

/**
 * Displays a mediaplayer using VLC plugin
 */
tvheadend.VLC = function(url) {
  var vlc = document.createElement('embed');
  vlc.setAttribute('type', 'application/x-vlc-plugin');
  vlc.setAttribute('pluginspage', 'http://www.videolan.org');
  vlc.setAttribute('version', 'version="VideoLAN.VLCPlugin.2');
  vlc.setAttribute('width', '507');
  vlc.setAttribute('height', '384');
  vlc.setAttribute('autoplay', 'yes');
  vlc.setAttribute('id', 'vlc');
  if(url) {
    vlc.setAttribute('src', url);
  } else {
    vlc.style.display = 'none';
  }
  
  var missingPlugin = document.createElement('div');
  missingPlugin.style.display = 'none';
  missingPlugin.style.padding = '5px';

  var selectChannel = new Ext.form.ComboBox({
    loadingText: 'Loading...',
    width: 200,
    displayField:'name',
    store: tvheadend.channels,
    mode: 'local',
    editable: false,
    triggerAction: 'all',
    emptyText: 'Select channel...'
  });
  
  selectChannel.on('select', function(c, r) {
      var url = 'stream/channelid/' + r.data.chid;
      var playlist = 'playlist/channelid/' + r.data.chid;
      var chName = r.data.name;
      if (!chName.length) {
	  chName = 'the channel';
      }

      if(!vlc.playlist || vlc.playlist == 'undefined') {
	  var innerHTML = '';
	  innerHTML += '<p>You are missing a plugin for your browser.'
	  innerHTML += 'You can still watch ' + chName;
	  innerHTML += ' using an external player.</p>';
	  innerHTML += '<p><a href="' + playlist + '">M3U Playlist</a></p>';
	  innerHTML += '<p><a href="' + url + '">Direct URL</a></p>';

	  missingPlugin.innerHTML = innerHTML;
	  missingPlugin.style.display = 'block';
	  return;
      }

      vlc.style.display = 'block';

      if(vlc.playlist && vlc.playlist.isPlaying) {
        vlc.playlist.stop();
      }
      if(vlc.playlist && vlc.playlist.items.count) {
        vlc.playlist.items.clear();
      }
      
      vlc.playlist.add(url, chName, "");
      vlc.playlist.play();
      vlc.audio.volume = slider.getValue();
    }
  );
  
  var slider = new Ext.Slider({
    width: 135,
    height: 20,
    value: 90,
    increment: 1,
    minValue: 0,
    maxValue: 100
  });
  
  var sliderLabel = new Ext.form.Label();
  sliderLabel.setText("90%");
  slider.addListener('change', function() {
    if(vlc.playlist && vlc.playlist.isPlaying) {
      vlc.audio.volume = slider.getValue();
      sliderLabel.setText(vlc.audio.volume + '%');
    } else {
      sliderLabel.setText(slider.getValue() + '%');
    }
  });
  
  var win = new Ext.Window({
    title: 'VLC Player',
    layout:'fit',
    width: 507 + 14,
    height: 384 + 56,
    constrainHeader: true,
    iconCls: 'eye',
    resizable: false,
    tbar: [
      selectChannel,
      '-',
      {
        iconCls: 'control_play',
        tooltip: 'Play',
        handler: function() {
          if(vlc.playlist && vlc.playlist.items.count && !vlc.playlist.isPlaying) {
            vlc.playlist.play();
          }
        }
      },
      {
        iconCls: 'control_pause',
        tooltip: 'Pause',
        handler: function() {
          if(vlc.playlist && vlc.playlist.items.count) {
            vlc.playlist.togglePause();
          }
        }
      },
      {
        iconCls: 'control_stop',
        tooltip: 'Stop',
        handler: function() {
          if(vlc.playlist && vlc.playlist.items.count) {
            vlc.playlist.stop();
            vlc.style.display = 'none';
          }
        }
      },
      '-',
      {
        iconCls: 'control_fullscreen',
        tooltip: 'Fullscreen',
        handler: function() {
          if(vlc.playlist && vlc.playlist.isPlaying) {
            vlc.video.toggleFullscreen();
          }
        }
      },
      '-',
      {
        iconCls: 'control_volume',
        tooltip: 'Volume',
        disabled: true
      },
    ],
    items: [vlc, missingPlugin]
  });
	  
  win.on('render', function() {
    win.getTopToolbar().add(slider);
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(sliderLabel);
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

    if(o.dvr == true && tvheadend.dvrpanel == null) {
	tvheadend.dvrpanel = new tvheadend.dvr;
	tvheadend.rootTabPanel.add(tvheadend.dvrpanel);
    }

    if(o.admin == true && tvheadend.confpanel == null) {
	tvheadend.confpanel = new Ext.TabPanel({
	    activeTab:0, 
	    autoScroll:true, 
	    title: 'Configuration', 
	    iconCls: 'wrench',
	    items: [new tvheadend.chconf,
		    new tvheadend.xmltv,
		    new tvheadend.cteditor,
		    new tvheadend.dvrsettings,
		    new tvheadend.tvadapters,
		    new tvheadend.iptv,
		    new tvheadend.acleditor, 
		    new tvheadend.cwceditor,
                    new tvheadend.capmteditor]
	});
	tvheadend.rootTabPanel.add(tvheadend.confpanel);
    }

    if(tvheadend.aboutPanel == null) {
	tvheadend.aboutPanel = new Ext.Panel({
	    border: false,
	    layout:'fit', 
	    title:'About',
	    iconCls:'info',
	    autoLoad: 'about.html'
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
// create application
tvheadend.app = function() {
 
    // public space
    return {
 
        // public methods
        init: function() {

	    tvheadend.rootTabPanel = new Ext.TabPanel({
		region:'center',
		activeTab:0,
		items:[new tvheadend.epg]
	    });

	    var viewport = new Ext.Viewport({
		layout:'border',
		items:[
		    {
			region:'south',
			contentEl: 'systemlog',
			split:true,
			autoScroll:true,
			height: 150,
			minSize: 100,
			maxSize: 400,
			collapsible: true,
			title:'System log',
			margins:'0 0 0 0',
			tools:[{
			    id:'gear',
			    qtip: 'Enable debug output',
			    handler: function(event, toolEl, panel){
				Ext.Ajax.request({
				    url: 'comet/debug',
				    params : { 
					boxid: tvheadend.boxid
				    }
				});
			    }
			}]
		    },tvheadend.rootTabPanel
		]
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
 
