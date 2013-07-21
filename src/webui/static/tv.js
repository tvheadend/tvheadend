
Ext.namespace('tv');
Ext.namespace('tv.ui');

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

if (VK_ESCAPE === undefined)
    var VK_ESCAPE = 0x1b;

if (VK_BACKSPACE === undefined)
    var VK_BACKSPACE = 0x08;

if (VK_SPACE === undefined)
    var VK_SPACE = 0x20;

if (VK_PAGE_UP === undefined)
    var VK_PAGE_UP = 0x21;

if (VK_PAGE_DOWN === undefined)
    var VK_PAGE_DOWN = 0x22;


tv.ui.VideoPlayer = Ext.extend(Ext.Panel, (function() {

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

    return {
	constructor: function(config) {
	    this.params = {};
	    tv.ui.VideoPlayer.superclass.constructor.call(this, config);

	    Ext.applyIf(this.params, {
		transcode : 0,
		resolution: 288,
		channels  : 0,         // same as source
		bandwidth : 0,         // same as source
		language  : '',        // same as source
		audio     : 'UNKNOWN', // same as source
		video     : 'UNKNOWN', // same as source
		subs      : 'UNKNOWN', // same as source
		muxer     : '',        // default dvr config
		playlist  : false      // don't use m3u8 playlist
	    });
	},

	initComponent: function() {
	    Ext.apply(this, {
		baseCls     : 'tv-video-player',
		bodyStyle   : 'background-color:#000;color:#fff',
		html        : '',
		bufferLength: 3000, //ms

		listeners: {
		    beforedestroy: {
			fn: function(dv, items) {
			    this.video = null;
			}
		    },
		    bodyresize: {
			fn: function(panel, width, height) {
			    this.video.setSize(width, height);
			}
		    },
		    render: {
			fn: function() {
			    this.video = this.body.createChild({
				tag     : 'video',
				width   : '100%',
				height  : '100%',
				html    : "Your browser doesn't support html5 video"
			    });
			    this.source = this.video.createChild({tag: 'source'});
			    this.source.dom.addEventListener('error', this.error.bind(this));

			    this.stop();

			    var self = this;
			    this.video.dom.addEventListener('loadeddata', function() {
				setTimeout(function() {
				    self.play();
				}, self.bufferLength); //buffer 3000ms
			    });
			}
		    }
		}
	    });
	    tv.ui.VideoPlayer.superclass.initComponent.apply(this, arguments);
	},

	_getUrl: function(chid, params) {
	    var url = '../';

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
	},

	_getProfile: function() {
	    var el = this.video.dom;

	    // chrome can handle h264+aac within mkv, given that h264 codecs are available
	    if(Ext.isChrome && 
	       el.canPlayType('video/mp4; codecs="avc1.42E01E, mp4a.40.2"') == 'probably')
		return profiles['mkv'];

	    for (var key in profiles)
		if(el.canPlayType(profiles[key].mimetype) == 'probably')
		    return profiles[key];
	    
	    for (var key in profiles)
		if(el.canPlayType(profiles[key].mimetype) == 'maybe')
		    return profiles[key];
	    
	    return {};
	},

	error: function() {
	    var url = this.source.dom.src;
	    if(url && url != document.location.href) {
		this.body.removeClass('tv-video-loading');
		this.body.removeClass('tv-video-idle');
		this.body.addClass('tv-video-error');
		this.video.hide();
	    }
	},

	stop: function() {
	    this.body.removeClass('tv-video-loading');
	    this.body.removeClass('tv-video-error');
	    this.body.addClass('tv-video-idle');
	    this.source.dom.src = '';
	    this.video.dom.load();
	},

	play: function() {
	    this.body.removeClass('tv-video-loading');
	    this.body.removeClass('tv-video-idle');
	    this.body.removeClass('tv-video-error');

	    this.video.show();
	    this.video.dom.play();
	},

	zapTo: function(chid, config) {
	    var config = config || {}
	    var params = {}

	    Ext.apply(params, this._getProfile(), this.params);
	    Ext.apply(params, config);

	    this.video.hide();
	    this.stop();

	    this.body.removeClass('tv-video-idle');
	    this.body.removeClass('tv-video-error');
	    this.body.addClass('tv-video-loading');

	    this.source.dom.src = this._getUrl(chid, params);
	    this.video.dom.load();
	}
    };
}()));


tv.ui.ChannelList = Ext.extend(Ext.DataView, {

    initComponent: function() {
	Ext.apply(this, {
	    cls: 'tv-list',
	    overClass: 'tv-list-item-over',
	    selectedClass: 'tv-list-item-selected',
	    itemSelector:'div.tv-list-item',
	    singleSelect: true,
	    tpl: new Ext.XTemplate(
		'<tpl for=".">',
		'<div class="tv-list-item" id="chid_{chid}">',
		'<img src="{ch_icon}" title="{name}">{name}',
		'</div>',
		'</tpl>'),

	    listeners: {
		selectionchange: {
		    fn: function(dv, items) {
			if(items.length == 0)
			    return;

			var node = this.getNode(items[0]);
			node = Ext.get(node);
			node.scrollIntoView(this.el);
		    }
		},
		dblclick: {
		    fn: function() {
			this.fireEvent('naventer');
		    }
		}
	    }
	});

	this.addEvents(
            'navup',
            'navdown',
	    'navleft',
	    'navright',
	    'navback',
	    'naventer'
        );
	tv.ui.ChannelList.superclass.initComponent.apply(this, arguments);
    },

    visibleItems: function() {
	var nodes = this.getNodes(0, 0);
	if(nodes.length == 0)
	    return 0;

	var height = this.getTemplateTarget().getHeight();
	var itemHeight = Ext.get(nodes[0]).getHeight()
	return Math.floor(height / itemHeight);
    },

    onRender : function(ct, position) {
        tv.ui.ChannelList.superclass.onRender.call(this, ct, position);
	Ext.dd.ScrollManager.register(this.el);

	this.getTemplateTarget().set({tabindex: Ext.id(undefined, '0')});
	this.getTemplateTarget().on('keydown', function(e) {
	    switch(e.getKey()) {

	    case VK_LEFT:
		this.fireEvent('navleft');
		break;

	    case VK_RIGHT:
		this.fireEvent('navright');
		break;

	    case VK_UP:
		this.fireEvent('navup', 1);
		break;

	    case VK_DOWN:
		this.fireEvent('navdown', 1);
		break;

	    case VK_PAGE_UP:
		var cnt = this.visibleItems();
		this.fireEvent('navup', cnt);
		break;

	    case VK_PAGE_DOWN:
		var cnt = this.visibleItems();
		this.fireEvent('navdown', cnt);
		break;

	    case VK_SPACE:
	    case VK_ENTER:
		this.fireEvent('naventer');
		break;

	    case VK_BACKSPACE:
	    case VK_ESCAPE:
	    case VK_BACK:
		this.fireEvent('navback');
		break;

	    default:
		return false;
	    }

	    e.stopEvent();
	    return true;

	}.bind(this));
    }
});


tv.app = function() {
    return {
	init: function() {

	    var videoPlayer = new tv.ui.VideoPlayer({
		params: {
		    resolution: 384
		},
		renderTo: Ext.getBody()
	    });

	    var chList = new tv.ui.ChannelList({
		store: new Ext.data.JsonStore({
		    autoLoad : true,
		    root : 'entries',
		    fields : ['ch_icon', 'number', 'name', 'chid'],
		    id : 'chid',
		    sortInfo : {
			field : 'number',
			direction : "ASC"
		    },
		    url : "../channels",
		    baseParams : {
			op : 'list'
		    }
		})
	    });

	    var chListPanel = new Ext.Panel({
		title:'Channels',
		items: chList,
		cls: 'tv-channel-list',
		renderTo: Ext.getBody()
	    });

	    window.onresize = function() {
		var h = chListPanel.el.getHeight();
		h -= chListPanel.header.getHeight();
		h -= 25;
		
		chList.setHeight(h);
	    };

	    chListPanel.on('show', function() {
		window.onresize();
	    });

	    chList.on('navback', function() {
		chListPanel.hide();
		chList.blur();
	    });

	    chList.on('naventer', function() {
		var indices = this.getSelectedIndexes();
		if(indices.length == 0)
		    return;
		    
		var item = this.store.getAt(indices[0]);

		videoPlayer.zapTo(item.data.chid);
		chListPanel.hide();
		chList.blur();
	    });

	    chList.on('navup', function(cnt) {
		var indices = chList.getSelectedIndexes();
		if(indices.length == 0)
		    this.select(this.store.getTotalCount() - 1);
		else if(indices[0] - cnt >= 0)
		    this.select(indices[0] - cnt);
		else
		    this.select(0);
	    });

	    chList.on('navdown', function(cnt) {
		var indices = chList.getSelectedIndexes();
		if(indices.length == 0)
		    this.select(0);
		else if(indices[0] + cnt < this.store.getTotalCount())
		    this.select(indices[0] + cnt);
		else
		    this.select(this.store.getTotalCount() - 1);
	    });

	    chList.on('navleft', function() {

	    });

	    chList.on('navright', function() {

	    });

	    var nav = new Ext.KeyNav(Ext.getDoc(), {
		'enter': function(e) {
		    chListPanel.show();
		    chList.focus();
		},
		'scope': this
	    });

	    chListPanel.show();
	    chList.focus();
	}
    };
}(); // end of app
