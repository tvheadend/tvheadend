
if (VK_LEFT === undefined)
    var VK_LEFT = 0x25;

if (VK_UP === undefined)
    var VK_UP = 0x26;

if (VK_RIGHT === undefined)
    var VK_RIGHT = 0x27;

if (VK_DOWN === undefined)
    var VK_DOWN = 0x28;

if (VK_ENTER === undefined)
    var VK_ENTER = 0x0d;

if (VK_PLAY === undefined)
    var VK_PLAY = 0x50;

if (VK_PAUSE === undefined)
    var VK_PAUSE = 0x51;

if (VK_STOP === undefined)
    var VK_STOP = 0x53;

if (VK_BACK === undefined)
    var VK_BACK = 0xa6;

Ext.namespace('tv');

tv.baseUrl = '../';

tv.channelTags = new Ext.data.JsonStore({
    autoLoad : true,
    root : 'entries',
    fields : [ 'identifier', 'name' ],
    id : 'identifier',
    sortInfo : {
	field : 'name',
	direction : "ASC"
    },
    url : tv.baseUrl + 'channeltags',
    baseParams : {
	op : 'listTags'
    }
});

tv.channels = new Ext.data.JsonStore({
    autoLoad : true,
    root : 'entries',
    fields : ['ch_icon', 'number', 'name', 'chid', 'tags'],
    id : 'chid',
    sortInfo : {
	field : 'number',
	direction : "ASC"
    },
    url : tv.baseUrl + "channels",
    baseParams : {
	op : 'list'
    }
});

tv.epg = new Ext.data.JsonStore({
    url : tv.baseUrl + "epg",
    bufferSize : 300,
    root : 'entries',
    fields : [ 'id' ,
	       'channel',
	       'channelid',
	       'title',
	       'subtitle',
	       'episode',
	       'description',
	       'chicon',
	       {
		   name : 'start',
		   type : 'date',
		   dateFormat : 'U' // unix time
	       }, 
	       {
		   name : 'end',
		   type : 'date',
		   dateFormat : 'U' // unix time
		}, 
	       'duration',
	       'contenttype',
	       'schedstate',
	       'serieslink',
	     ],
    baseParams : {
	limit : 2
    }
});


tv.ui = function() {
    //private space
 
    // public space
    return {
    };

}(); // end of ui



tv.playback = function() {
    //private space

    var profiles = {
	pass: {
	    muxer:   'pass',
	    mimetype: 'video/MP2T'
	},
	hls: {
	    muxer:     'mpegts',
	    transcode: true,
	    audio:     'AAC',
	    video:     'H264',
	    subs:      'NONE',
	    playlist:  true,
	    mimetype:  'application/x-mpegURL; codecs="avc1.42E01E, mp4a.40.2"'
	},
	apple: {
	    muxer:     'mpegts',
	    transcode: true,
	    audio:     'AAC',
	    video:     'H264',
	    subs:      'NONE',
	    playlist:  true,
	    mimetype:  'application/vnd.apple.mpegURL; codecs="avc1.42E01E, mp4a.40.2"'
	},
	ts: {
	    muxer:     'mpegts',
	    transcode: true,
	    audio:     'AAC',
	    video:     'H264',
	    subs:      'NONE',
	    playlist:  false,
	    mimetype:  'video/MP2T; codecs="avc1.42E01E, mp4a.40.2"'
	},
	mkv: {
	    muxer:     'matroska',
	    transcode: true,
	    audio:     'AAC',
	    video:     'H264',
	    subs:      'NONE',
	    playlist:  false,
	    mimetype:  'video/x-matroska; codecs="avc1.42E01E, mp4a.40.2"'
	},
	webm: {
	    muxer:     'webm',
	    transcode: true,
	    audio:     'VORBIS',
	    video:     'VP8',
	    subs:      'NONE',
	    mimetype:  'video/webm; codecs="vp8.0, vorbis"'
	}
    };

    // public space
    return {
	getProfile: function() {
	    var vid = document.createElement('video');

	    // chrome can handle h264+aac within mkv, given that h264 codecs are available
	    if(Ext.isChrome && 
	       vid.canPlayType('video/mp4; codecs="avc1.42E01E, mp4a.40.2"') == 'probably')
		return profiles['mkv'];

	    for (var key in profiles)
		if(vid.canPlayType(profiles[key].mimetype) == 'probably')
		    return profiles[key];

	    for (var key in profiles)
		if(vid.canPlayType(profiles[key].mimetype) == 'maybe')
		    return profiles[key];

	    return {};
	},

	getUrl: function(chid, config) {
	    var config = config || {}
	    var params = {}

	    Ext.apply(params, this.getProfile(), {
		transcode : 0,
		resolution: 384,
		channels  : 0,         // same as source
		bandwidth : 0,         // same as source
		language  : '',        // same as source
		audio     : 'UNKNOWN', // same as source
		video     : 'UNKNOWN', // same as source
		subs      : 'UNKNOWN', // same as source
		muxer     : '',        // default dvr config
		playlist  : false      // don't use m3u8 playlist
	    });
	    Ext.apply(params, config);
	    
	    var url = tv.baseUrl;

	    if(params.playlist)
		url += 'playlist/channelid/'
	    else
		url += 'stream/channelid/'

	    url += chid;
	    url += "?transcode="  + new Number(params.transcode);
	    url += "&mux="        + params.muxer;
	    url += "&acodec="     + params.audio;
	    url += "&vcodec="     + params.video;
	    url += "&scodec="     + params.subs;
	    url += "&resolution=" + params.resolution;
	    url += "&bandwidth="  + params.bandwidth;
	    url += "&language="   + params.language;

	    return url;
	}
    };

}(); // end of player

tv.app = function() {
    //private space

    var updateClock = new Ext.util.DelayedTask(function() {
	var clock = Ext.get('sb_currentTime');
	clock.update(new Date().toLocaleTimeString());
	updateClock.delay(1000);
    });

    var currentChannel = -1;
    var initKeyboard = function() {

	var channelLogoEl = document.getElementById('sb_channelLogo');
	channelLogoEl.onclick = function() {
	    currentChannel += 1;
	    ch = tv.channels.getAt(currentChannel);
	    onChannelZap(ch.data);
	}

	document.onkeyup = function(e) {
	    switch(e.keyCode) {

	    case VK_UP:
		var max = tv.channels.getTotalCount() - 1;
		if(max < 0)
		    return;

		else if(currentChannel < max)
		    currentChannel += 1;

		else
		    currentChannel = max;
		
		ch = tv.channels.getAt(currentChannel);
		onChannelZap(ch.data);
		break;

	    case VK_DOWN:
		if(tv.channels.getTotalCount() == 0)
		    return;

		else if(currentChannel < 1)
		    currentChannel = 0;

		else
		    currentChannel -= 1;

		ch = tv.channels.getAt(currentChannel);
		onChannelZap(ch.data);
		break;
	    }
	};
    };

    var onChannelZap = function(ch) {
	var channelNameEl = document.getElementById('sb_channelName');
	channelNameEl.innerHTML = ch.name;

	var channelLogoEl = document.getElementById('sb_channelLogo');
	if(ch.ch_icon) {
	    channelLogoEl.src = ch.ch_icon;
	    channelLogoEl.style = '';
	} else {
	    channelLogoEl.src = '';
	    channelLogoEl.style = 'display: none';
	}
	
	tv.epg.setBaseParam('channel', new String(ch.name));
	tv.epg.load();

	var videoPlayer = document.getElementById('tv_videoPlayer');
	
	setTimeout(function() {
	    videoPlayer.src = tv.playback.getUrl(ch.chid);
	    videoPlayer.load();
	    videoPlayer.play();
	}, 1);
    }

    var initDataStores = function() {
	tv.epg.on('load', function() {
	    var nowTitle = document.getElementById('sb_nowTitle');
	    var nowDuration = document.getElementById('sb_nowDuration');
	    var nextTitle = document.getElementById('sb_nextTitle');
	var nextDuration = document.getElementById('sb_nextDuration');
	    var event = undefined;
	    
	    if(tv.epg.getTotalCount() < 1) {
		nowTitle.innerHTML = '';
		nowDuration.innerHTML = '';
	    } else {
		event = tv.epg.getAt(0).data;
		nowTitle.innerHTML = event.title;
	    nowDuration.innerHTML = event.start.toLocaleTimeString();
	    }
	    
	    if(tv.epg.getTotalCount() < 2) {
		nextTitle.innerHTML = '';
		nextDuration.innerHTML = '';
	    } else {
		event = tv.epg.getAt(1).data;
		nextTitle.innerHTML = event.title;
		nextDuration.innerHTML = event.start.toLocaleTimeString();
	    }
	});
    };

    // public space
    return {
	init: function() {
	    initDataStores();
	    initKeyboard();
	    updateClock.delay(1);
	}
    };
}(); // end of app
