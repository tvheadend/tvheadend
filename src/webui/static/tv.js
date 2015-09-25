Ext.namespace('tv');
Ext.namespace('tv.ui');

/* Define key code mappings for navigation keys */

if (VK_END === undefined)
    var VK_END = 0x23;

if (VK_HOME === undefined)
    var VK_HOME = 0x24;

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
            profile:   'pass',
            mimetype:  'video/MP2T'
        },
        webm: {
            profile:   'webtv-vp8-vorbis-webm',
            playlist:  false,
            mimetype:  'video/webm; codecs="vp8.0, vorbis"'
        },
        hls: {
            profile:   'webtv-h264-aac-mpegts',
            playlist:  true,
            mimetype:  'application/x-mpegURL; codecs="avc1.42E01E, mp4a.40.2"'
        },
        apple: {
            profile:   'webtv-h264-aac-mpegts',
            playlist:  true,
            mimetype:  'application/vnd.apple.mpegURL; codecs="avc1.42E01E, mp4a.40.2"'
        },
        ts: {
            profile:   'webtv-h264-aac-mpegts',
            playlist:  false,
            mimetype:  'video/MP2T; codecs="avc1.42E01E, mp4a.40.2"'
        },
        mkv: {
            profile:   'webtv-h264-aac-matroska',
            playlist:  false,
            mimetype:  'video/x-matroska; codecs="avc1.42E01E, mp4a.40.2"'
        }
    };

    return {
        constructor: function(config) {
            this.params = {};
            tv.ui.VideoPlayer.superclass.constructor.call(this, config);

            Ext.applyIf(this.params, {
                profile   : '',        // stream profile
                playlist  : false      // don't use m3u8 playlist
            });
        },

        initComponent: function() {
            Ext.apply(this, {
                baseCls     : 'tv-video-player',
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
                            this.message = this.body.createChild({
                                tag : 'div',
                                cls : 'tv-video-message',
                                html: ''
                            });
                            this.message.setVisibilityMode(Ext.Element.DISPLAY);
                            this.message.hide();

                            this.video = this.body.createChild({
                                tag     : 'video',
                                html    : "Your browser doesn't support html5 video"
                            });

                            this.source = this.video.createChild({tag: 'source'});
                            this.source.dom.addEventListener('error', this.error.bind(this));

                            this.stop();

                            var self = this;
                            this.video.dom.addEventListener('error', this.error.bind(this));
                            this.video.dom.addEventListener('loadeddata', function() {
                                setTimeout(function() {
                                    self.play();
                                },
                                self.bufferLength);
                            });
                        }
                    }
                }
            });
            tv.ui.VideoPlayer.superclass.initComponent.apply(this, arguments);
        },

        _getUrl: function(uuid, params) {
            var url = '';

            if(params.playlist)
                url += 'playlist/channel/'
            else
                url += 'stream/channel/'

            url += uuid;

            if (params.profile)
                url += "?profile=" + params.profile;

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

        error: function(e) {
            var url = this.source.dom.src;

            if(url && url != document.location.href) {
                this.body.removeClass('tv-video-loading');
                this.body.removeClass('tv-video-idle');
                this.body.addClass('tv-video-error');
                this.video.hide();

                this.message.update('An unknown error occurred.');

                var err = e.target.error;

                if(err) {
                    switch (err.code) {

                        case err.MEDIA_ERR_ABORTED:
                            this.message.update('You aborted the video playback.');
                            break;

                        case err.MEDIA_ERR_NETWORK:
                            this.message.update('A network error caused the video ' +
                                'download to fail part-way.');
                            break;

                        case err.MEDIA_ERR_DECODE:
                            this.message.update('The video playback was aborted due to ' +
                                'a corruption problem or because the video ' +
                                'used features your browser did not support.');
                            break;

                        case err.MEDIA_ERR_SRC_NOT_SUPPORTED:
                            this.message.update('The video could not be loaded, either because ' +
                                'the server or network failed or because the ' +
                                'format is not supported.');
                            break;
                    }
                }
                this.message.show();
            }
        },

        stop: function() {
            this.message.hide();
            this.body.removeClass('tv-video-loading');
            this.body.removeClass('tv-video-error');
            this.body.addClass('tv-video-idle');
            this.source.dom.src = '';
            this.video.dom.load();
        },

        pause: function() {
            this.video.dom.pause();
        },

        muteToggle: function() {
            this.video.dom.muted = !this.video.dom.muted;
            return this.video.dom.muted;
        },

        setVolume: function(vol) {
            this.video.dom.volume = vol / 100.0;
        },

        getVolume: function() {
            return Math.round(100 * this.video.dom.volume);
        },

        setDisplaySize: function(width, height) {
            this.video.setSize(width, height);
        },

        setProfile: function(pro) {
            this.params.profile = pro;
        },

        isIdle: function() {
            return this.body.hasClass('tv-video-idle');
        },

        fullscreen: function() {
            var dom  = this.video.dom;

            if(typeof dom.requestFullScreen !== 'undefined')
                dom.requestFullScreen();

            else if(typeof dom.mozRequestFullScreen !== 'undefined')
                dom.mozRequestFullScreen();

            else if(typeof dom.webkitRequestFullScreen !== 'undefined')
                dom.webkitEnterFullscreen();
        },

        play: function() {
            this.message.hide();
            this.body.removeClass('tv-video-loading');
            this.body.removeClass('tv-video-idle');
            this.body.removeClass('tv-video-error');

            this.video.show();
            this.video.dom.play();
        },

        zapTo: function(uuid, config) {
            var config = config || {}
            var params = {}

            if (!this.params.profile)
                Ext.apply(params, this._getProfile(), this.params);
            else
                Ext.apply(params, this.params);

            Ext.apply(params, config);

            this.video.hide();
            this.stop();

            this.message.update('Loading...');
            this.message.show();

            this.body.removeClass('tv-video-idle');
            this.body.removeClass('tv-video-error');
            this.body.addClass('tv-video-loading');

            this.source.dom.src = this._getUrl(uuid, params);
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
                '<div class="tv-list-item" id="{uuid}">',
                '<img src="{icon_public_url}" title="{name}">{name}',
                '</div>',
                '</tpl>'
            ),

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
            'navback',
            'naventer',
            'pageup',
            'pagedown',
            'pagefirst',
            'pagelast'
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

                case VK_UP:
                    this.fireEvent('navup');
                    break;

                case VK_DOWN:
                    this.fireEvent('navdown');
                    break;

                case VK_LEFT:
                case VK_PAGE_UP:
                    this.fireEvent('pageup');
                    break;

                case VK_RIGHT:
                case VK_PAGE_DOWN:
                    this.fireEvent('pagedown');
                    break;

                case VK_HOME:
                    this.fireEvent('pagefirst');
                    break;

                case VK_END:
                    this.fireEvent('pagelast');
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

            var channelStore = new Ext.data.JsonStore({
                autoLoad: {params:{start: 0, limit: 8}}, // limit initial page size to 8
                root : 'entries',
                fields : ['icon_public_url', 'number', 'name', 'uuid'],
                id : 'uuid',
                remoteSort: true,
                sortInfo : {
                    field : 'number', // WIBNI: Ideally, sort the whole channel list at source
                    direction : "ASC"
                },
                url : "api/channel/grid"
            });

            var videoPlayer = new tv.ui.VideoPlayer({
                params: { },
                renderTo: Ext.getBody()
            });

            videoPlayer.setDisplaySize('100%', '00%');

            var chList = new tv.ui.ChannelList({
                autoScroll: true,
                store: channelStore
            });

    // Play button that calls the "I've pressed Enter!" event when clicked

            var playButton = new Ext.Button({
                text: 'Play Selected Channel',
                handler: function() {
                    chList.fireEvent('naventer');
                }
            });

    // Paging bar so you can move through the list of channels

            var pageBar = new Ext.PagingToolbar({
                store: channelStore,
                pageSize: 8, // replicates initial page size
                listeners: {
                    change : function(scope, params) {
                         totalPages = params.pages;
                         activePage = params.activePage;
                         lastPageSize = ( params.total % scope.pageSize );
                         }
                }
            });

            var chListPanel1 = new Ext.Panel({
                items: [ pageBar, playButton ],
                cls: 'tv-channel-list-header'
            });

            var chListPanel2 = new Ext.Panel({
                items: [ chList ],
                cls: 'tv-channel-list-content'
            });

            var chListPanel = new Ext.Panel({
                title:'Channels',
                items: [ chListPanel1, chListPanel2 ],
                cls: 'tv-channel-list',
                renderTo: Ext.getBody()
            });

            window.onresize = function() {
                var h = chListPanel.el.getHeight();
                h -= chListPanel.header.getHeight();
                h -= 250;
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
                videoPlayer.zapTo(item.id);
                chListPanel.hide();
                chList.blur();
            });

            chList.on('navup', function() {
                var indices = chList.getSelectedIndexes();

                if((indices.length == 0) && (activePage == totalPages))
                    this.select(lastPageSize - 1);
                else if(indices.length == 0)
                    this.select(pageBar.pageSize - 1);
                else if(indices[0] - 1 >= 0)
                    this.select(indices[0] - 1);
                else
                    this.select(0);
            });

            chList.on('navdown', function() {
                var indices = chList.getSelectedIndexes();

                if(indices.length == 0)
                    this.select(0);
                else if((activePage == totalPages) && (indices[0] + 1 == lastPageSize))
                    this.select(lastPageSize - 1);
                else if(indices[0] + 1 < pageBar.pageSize)
                    this.select(indices[0] + 1);
                else
                    this.select(pageBar.pageSize - 1);
            });

            chList.on('pageup', function() {
                if (activePage !== 1)
                    pageBar.movePrevious();
            });

            chList.on('pagedown', function() {
                if ( activePage !== totalPages)
                    pageBar.moveNext();
            });

            chList.on('pagefirst', function() {
                pageBar.moveFirst();
            });

            chList.on('pagelast', function() {
                pageBar.moveLast();
            });

            Ext.getDoc().on('keydown', function(e) {
                switch(e.getKey()) {
                    case VK_ENTER:

                        var video = document.getElementById(videoPlayer.video.id);

                        if (video.hasAttribute("controls"))
                            video.removeAttribute("controls");
                        else
                            video.setAttribute("controls",true);
                    break;

                    case VK_ESCAPE:
                    case VK_BACK:
                    case VK_BACKSPACE:
                        chListPanel.show();
                        chList.focus();
                    break;
                }
            });

            chListPanel.show();
            chList.focus();
        }
    };
}(); // end of app
