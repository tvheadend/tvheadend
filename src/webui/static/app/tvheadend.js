
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
 
  function randomString() {
	var chars = "ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz";
	var string_length = 8;
	var randomstring = '';
	for (var i=0; i<string_length; i++) {
		var rnum = Math.floor(Math.random() * chars.length);
		randomstring += chars.substring(rnum,rnum+1);
	}
	return randomstring;
  }
  var vlc = document.createElement('embed');
  vlc.setAttribute('type', 'application/x-vlc-plugin');
  vlc.setAttribute('pluginspage', 'http://www.videolan.org');
  vlc.setAttribute('version', 'VideoLAN.VLCPlugin.2');
  vlc.setAttribute('width', '100%');
  vlc.setAttribute('height', '100%');
  vlc.setAttribute('autoplay', 'no');
  vlc.setAttribute('id', randomString());
  
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
      var streamurl = 'stream/channelid/' + r.data.chid;
      var playlisturl = 'playlist/channelid/' + r.data.chid;

      // if the player was initialised, but not yet shown, make it visible
      if (vlc.playlist && (vlc.style.display == 'none'))
	vlc.style.display = 'block';

      if(!vlc.playlist || vlc.playlist == 'undefined') {
	 missingPlugin.innerHTML  = '<p>Embedded player could not be started. <br> You are probably missing VLC Mozilla plugin for your browser.</p>';
	 missingPlugin.innerHTML += '<p><a href="' + playlisturl + '">M3U Playlist</a></p>';
	 missingPlugin.innerHTML += '<p><a href="' + streamurl + '">Direct URL</a></p>';
      }
      else {
        vlc.playlist.stop();
   	 vlc.playlist.items.clear();
   	 vlc.playlist.add(streamurl);
    	 vlc.playlist.playItem(0);
     	 vlc.audio.volume = slider.getValue();
	}
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
    resizable: true,
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
          if(vlc.playlist) {
            vlc.playlist.stop();
          }
        }
      },
      '-',
      {
        iconCls: 'control_fullscreen',
        tooltip: 'Fullscreen',
        handler: function() {
          if(vlc.playlist && vlc.playlist.isPlaying && (vlc.VersionInfo.substr(0,3) != '1.1')) {
            vlc.video.toggleFullscreen();
          }
	   else if (vlc.VersionInfo.substr(0,3) == '1.1') {
            alert('Fullscreen mode is broken in VLC 1.1.x');
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
	  
  win.on('beforeShow', function() {
    win.getTopToolbar().add(slider);
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(new Ext.Toolbar.Spacer());
    win.getTopToolbar().add(sliderLabel);

    // check if vlc plugin wasn't initialised correctly
    if(!vlc.playlist || (vlc.playlist == 'undefined')) {
      vlc.style.display = 'none';
      
      missingPlugin.innerHTML  = '<p>Embedded player could not be started. <br> You are probably missing VLC Mozilla plugin for your browser.</p>';
      
      if (url) {
        var channelid = url.substr(url.lastIndexOf('/'));
        var streamurl = 'stream/channelid/' + channelid;
        var playlisturl = 'playlist/channelid/' + channelid;
        missingPlugin.innerHTML += '<p><a href="' + playlisturl + '">M3U Playlist</a></p>';
        missingPlugin.innerHTML += '<p><a href="' + streamurl + '">Direct URL</a></p>';
      }

      missingPlugin.style.display = 'block';
    }
    else {
	// check if the window was opened with an url-parameter
	if (url) {
	  vlc.playlist.items.clear();
	  vlc.playlist.add(url);
	  vlc.playlist.playItem(0);
	  
	  //enable yadif2x deinterlacer for vlc > 1.1
	  var point1 = vlc.VersionInfo.indexOf('.');
	  var point2 = vlc.VersionInfo.indexOf('.', point1+1);
	  var majVersion = vlc.VersionInfo.substring(0,point1);
	  var minVersion = vlc.VersionInfo.substring(point1+1,point2);
	  if ((majVersion >= 1) && (minVersion >= 1))
	    vlc.video.deinterlace.enable("yadif2x");	  
	}
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
 
