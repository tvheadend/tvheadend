
tvheadend.dynamic = true;
tvheadend.accessupdate = null;
tvheadend.capabilities = null;

/* State Provider */
Ext.state.Manager.setProvider(new Ext.state.CookieProvider({
    // 7 days from now
    expires: new Date(new Date().getTime() + (1000 * 60 * 60 * 24 * 7))
}));

tvheadend.regexEscape = function(s) {
    return s.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
}

/**
 * Displays a help popup window
 */
tvheadend.help = function(title, pagename) {
    Ext.Ajax.request({
        url: 'redir/docs/' + pagename,
        success: function(result, request) {

            var content = new Ext.Panel({
                autoScroll: true,
                border: false,
                layout: 'fit',
                html: result.responseText
            });

            var win = new Ext.Window({
                title: _('Help for') + ' ' + title,
                iconCls: 'help',
                layout: 'fit',
                width: 900,
                height: 400,
                constrainHeader: true,
                items: [content]
            });
            win.show();

        }
    });
};

tvheadend.paneladd = function(dst, add, idx) {
    if (idx != null)
        dst.insert(idx, add);
    else
        dst.add(add);
};

tvheadend.panelreg = function(tabpanel, panel, builder, destroyer) {
    /* the 'activate' event does not work in ExtJS 3.4 */
    tabpanel.on('beforetabchange', function(tp, p) {
        if (p == panel)
            builder();
    });
    panel.on('deactivate', destroyer);
}

tvheadend.Ajax = function(conf) {
    var orig_success = conf.success;
    var orig_failure = conf.failure;
    conf.success = function(d) {
        tvheadend.loading(0);
        if (orig_success)
            orig_success(d);
    }
    conf.failure = function(d) {
        tvheadend.loading(0);
        if (orig_failure)
            orig_failure(d);
    }
    tvheadend.loading(1);
    Ext.Ajax.request(conf);
};

tvheadend.AjaxConfirm = function(conf) {
    Ext.MessageBox.confirm(
        conf.title || _('Message'),
        conf.question || _('Do you really want to delete the selection?'),
        function (btn) {
            if (btn == 'yes')
                tvheadend.Ajax(conf);
        }
    );
};

tvheadend.loading = function(on) {
    if (on)
      Ext.getBody().mask(_('Loading... Please, wait...'), 'loading');
    else
      Ext.getBody().unmask();
};

tvheadend.PagingToolbarConf = function(conf, title, auto, count)
{
  conf.width = 50;
  conf.pageSize = 50;
  conf.displayInfo = true;
                    /// {0} start, {1} end, {2} total, {3} title
  conf.displayMsg = _('{3} {0} - {1} of {2}').replace('{3}', title);
                    /// {0} title (lowercase), {1} title
  conf.emptyMsg = String.format(_('No {0} to display'), title.toLowerCase(), title);
  conf.items = [];
  if (auto || count)
    conf.items.push('-');
  if (auto) {
    conf.items.push(_('Auto-refresh'));
    conf.items.push(auto);
  }
  if (count) {
    conf.items.push('->');
    conf.items.push('-');
    conf.items.push(_('Per page'));
    conf.items.push(count);
  }
  return conf;
}

/*
 * Any Match option in ComboBox queries
 * This query is identical as in extjs-all.js
 * except one
 */
tvheadend.doQueryAnyMatch = function(q, forceAll) {
    q = Ext.isEmpty(q) ? '' : q;
    var qe = {
        query: q,
        forceAll: forceAll,
        combo: this,
        cancel:false
    };

    if (this.fireEvent('beforequery', qe) === false || qe.cancel)
        return false;

    q = qe.query;
    forceAll = qe.forceAll;
    if (forceAll === true || (q.length >= this.minChars)) {
        if (this.lastQuery !== q) {
            this.lastQuery = q;
            if (this.mode == 'local') {
                this.selectedIndex = -1;
                if (forceAll) {
                    this.store.clearFilter();
                } else {
                    /* supply the anyMatch option (last param) */
                    this.store.filter(this.displayField, q, true);
                }
                this.onLoad();
            } else {
                this.store.baseParams[this.queryParam] = q;
                this.store.load({ params: this.getParams(q) });
                this.expand();
            }
        } else {
            this.selectedIndex = -1;
            this.onLoad();
        }
    }
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

/*
 *
 */
tvheadend.niceDate = function(dt) {
    var d = new Date(dt);
    return '<div class="x-nice-dayofweek">' + d.toLocaleString(window.navigator.language, {weekday: 'long'}) + '</div>' +
           '<div class="x-nice-date">' + d.toLocaleDateString() + '</div>' +
           '<div class="x-nice-time">' + d.toLocaleTimeString() + '</div>';
}

/**
 * Displays a mediaplayer using the html5 video element
 */
tvheadend.VideoPlayer = function(url) {

    var videoPlayer = new tv.ui.VideoPlayer({
        params: { }
    });

    var selectChannel = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 200,
        displayField: 'val',
        store: tvheadend.channels,
        mode: 'local',
        editable: true,
        triggerAction: 'all',
        emptyText: _('Select channel...')
    });

    selectChannel.on('select', function(c, r) {
        videoPlayer.zapTo(r.id);
    });

    var slider = new Ext.Slider({
        width: 135,
        height: 20,
        value: 90,
        increment: 1,
        minValue: 0,
        maxValue: 100
    });

    var sliderLabel = new Ext.form.Label();
    sliderLabel.setText('90%');
    slider.addListener('change', function() {
        videoPlayer.setVolume(slider.getValue());
        sliderLabel.setText(videoPlayer.getVolume() + '%');
    });

    if (!tvheadend.profiles) {
        tvheadend.profiles = tvheadend.idnode_get_enum({
            url: 'api/profile/list',
            event: 'profile'
        });
    }

    var selectProfile = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 150,
        displayField: 'val',
        mode: 'local',
        editable: false,
        triggerAction: 'all',
        emptyText: _('Select stream profile...'),
        store: tvheadend.profiles
    });

    selectProfile.on('select', function(c, r) {
        videoPlayer.setProfile(r.data.val);
        if (videoPlayer.isIdle())
            return;

        var index = selectChannel.selectedIndex;
        if (index < 0)
            return;

        var ch = selectChannel.getStore().getAt(index);
        videoPlayer.zapTo(ch.id);
    });

    var win = new Ext.Window({
        title: _('Live TV Player'),
        layout: 'fit',
        width: 682 + 14,
        height: 384 + 56,
        constrainHeader: true,
        iconCls: 'watchTv',
        resizable: true,
        tbar: [
            selectChannel,
            '-',
            {
                iconCls: 'control_play',
                tooltip: _('Play'),
                handler: function() {
                    if (!videoPlayer.isIdle()) { //probobly paused
                        videoPlayer.play();
                        return;
                    }

                    var index = selectChannel.selectedIndex;
                    if (index < 0)
                        return;

                    var ch = selectChannel.getStore().getAt(index);
                    videoPlayer.zapTo(ch.id);
                }
            },
            {
                iconCls: 'control_pause',
                tooltip: _('Pause'),
                handler: function() {
                    videoPlayer.pause();
                }
            },
            {
                iconCls: 'control_stop',
                tooltip: _('Stop'),
                handler: function() {
                    videoPlayer.stop();
                }
            },
            '-',
            {
                iconCls: 'control_fullscreen',
                tooltip: _('Fullscreen'),
                handler: function() {
                    videoPlayer.fullscreen();
                }
            },
            '-',
            selectProfile,
            '-',
            {
                iconCls: 'control_volume',
                tooltip: _('Volume'),
                disabled: true
            }],
        items: [videoPlayer]
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

function diskspaceUpdate(o) {
  if ('freediskspace' in o && 'totaldiskspace' in o)
    tvheadend.rootTabPanel.setDiskSpace(o.freediskspace, o.totaldiskspace);
}

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

    if ('username' in o)
      tvheadend.rootTabPanel.setLogin(o.username);
    if ('address' in o)
      tvheadend.rootTabPanel.setAddress(o.address);
    if ('freediskspace' in o && 'totaldiskspace' in o)
      tvheadend.rootTabPanel.setDiskSpace(o.freediskspace, o.totaldiskspace);

    if (tvheadend.autorecButton)
        tvheadend.autorecButton.setDisabled(o.dvr != true);

    if (o.dvr == true && tvheadend.dvrpanel == null) {
        tvheadend.dvrpanel = tvheadend.dvr();
        tvheadend.rootTabPanel.add(tvheadend.dvrpanel);
    }

    if (o.admin == true && tvheadend.confpanel == null) {

        var cp = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('Configuration'),
            iconCls: 'wrench',
            items: []
        });

        tvheadend.miscconf(cp);

        tvheadend.acleditor(cp);
        tvheadend.passwdeditor(cp);

        /* DVB inputs, networks, muxes, services */
        var dvbin = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('DVB Inputs'),
            iconCls: 'hardware',
            items: []
        });

        var idx = 0;

        if (tvheadend.capabilities.indexOf('tvadapters') !== -1)
            tvheadend.tvadapters(dvbin);
        tvheadend.networks(dvbin);
        tvheadend.muxes(dvbin);
        tvheadend.services(dvbin);
        tvheadend.mux_sched(dvbin);

        cp.add(dvbin);

        /* Channel / EPG */
        var chepg = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('Channel / EPG'),
            iconCls: 'television',
            items: []
        });
        tvheadend.channel_tab(chepg);
        tvheadend.cteditor(chepg);
        tvheadend.bouquet(chepg);
        tvheadend.epggrab(chepg);

        cp.add(chepg);

        /* Stream Config */
        var stream = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('Stream'),
            iconCls: 'film_edit',
            items: []
        });
        tvheadend.esfilter_tab(stream);

        cp.add(stream);

        /* DVR / Timeshift */
        var tsdvr = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('Recording'),
            iconCls: 'recordingtab',
            items: []
        });
        tvheadend.dvr_settings(tsdvr);
        if (tvheadend.capabilities.indexOf('timeshift') !== -1)
          tvheadend.timeshift(tsdvr);

        cp.add(tsdvr);

        /* CSA */
        if (tvheadend.capabilities.indexOf('caclient') !== -1)
            tvheadend.caclient(cp, null);

        /* Debug */
        tvheadend.tvhlog(cp);

        /* Finish */
        tvheadend.rootTabPanel.add(cp);
        tvheadend.confpanel = cp;
        cp.doLayout();
    }

    if (o.admin == true && tvheadend.statuspanel == null) {
        tvheadend.statuspanel = new tvheadend.status;
        tvheadend.rootTabPanel.add(tvheadend.statuspanel);
    }

    if (tvheadend.aboutPanel == null) {
        tvheadend.aboutPanel = new Ext.Panel({
            border: false,
            layout: 'fit',
            autoScroll: true,
            title: _('About'),
            iconCls: 'info',
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
    s = style ? '<div style="' + style + '">' : '<div>';

    sl = Ext.get('systemlog');
    e = Ext.DomHelper.append(sl, s + '<pre>' + msg + '</pre></div>');
    e.scrollIntoView('systemlog');
};

/**
 *
 */
tvheadend.RootTabPanel = Ext.extend(Ext.TabPanel, {

    onRender: function(ct, position) {
        tvheadend.RootTabPanel.superclass.onRender.call(this, ct, position);

        /* Create login components */
        var before = this.strip.dom.childNodes[this.strip.dom.childNodes.length-1];

        if (!this.loginTpl) {
            var tt = new Ext.Template(
                '<li class="x-tab-login" id="{id}">',
                '<span class="x-tab-strip-login {iconCls}">{text}</span></li>'
            );
            tt.disableFormats = true;
            tt.compile();
            tvheadend.RootTabPanel.prototype.loginTpl = tt;
        }
        var item = new Ext.Component();
        var p = this.getTemplateArgs(item);
        var before = this.strip.dom.childNodes[this.strip.dom.childNodes.length-1];
        item.tabEl = this.loginTpl.insertBefore(before, p);
        this.loginItem = item;

        if (!this.loginCmdTpl) {
            var tt = new Ext.Template(
                '<li class="x-tab-login" id="{id}"><a href="#">',
                '<span class="x-tab-strip-login-cmd"></span></a></li>'
            );
            tt.disableFormats = true;
            tt.compile();
            tvheadend.RootTabPanel.prototype.loginCmdTpl = tt;
        }
        var item = new Ext.Component();
        var p = this.getTemplateArgs(item);
        var el = this.loginCmdTpl.insertBefore(before, p);
        item.tabEl = Ext.get(el);
        item.tabEl.select('a').on('click', this.onLoginCmdClicked, this, {preventDefault: true});
        this.loginCmdItem = item;

        if (!this.diskSpaceTpl) {
            var tt = new Ext.Template(
                '<li class="x-tab-login" id="{id}">',
                '<span class="x-tab-diskspace"></span></li>'
            );
            tt.disableFormats = true;
            tt.compile();
            tvheadend.RootTabPanel.prototype.diskSpaceTpl = tt;
        }
        var item = new Ext.Component();
        var p = this.getTemplateArgs(item);
        var el = this.diskSpaceTpl.insertBefore(before, p);
        item.tabEl = Ext.get(el);
        this.diskSpaceItem = item;

        this.on('beforetabchange', function(tp, p) {
            if (p == this.loginItem || p == this.loginCmdItem || p == this.diskSpaceItem)
                return false;
        });
    },

    getComponent: function(comp) {
        if (comp === this.loginItem.id || comp == this.loginItem)
            return this.loginItem;
        if (comp === this.loginCmdItem.id || comp == this.loginCmdItem)
            return this.loginCmdItem;
        if (comp === this.diskSpaceItem.id || comp == this.diskSpaceItem)
            return this.diskSpaceItem;
        return tvheadend.RootTabPanel.superclass.getComponent.call(this, comp);
    },

    setLogin: function(login) {
        this.login = login;
        if (login) {
            text = _('Logged in as') + ' <b>' + login + '</b>';
            cmd = '(' + _('logout') + ')';
        } else {
            text = _('No verified access');
            cmd = '(' + _('login') + ')';
        }
        var el = this.loginItem.tabEl;
        var fly = Ext.fly(this.loginItem.tabEl);
        var t = fly.child('span.x-tab-strip-login', true);
        Ext.fly(this.loginItem.tabEl).child('span.x-tab-strip-login', true).innerHTML = text;
        Ext.fly(this.loginCmdItem.tabEl).child('span.x-tab-strip-login-cmd', true).innerHTML = cmd;
    },

    setAddress: function(addr) {
        Ext.get(this.loginItem.tabEl).child('span.x-tab-strip-login', true).qtip = addr;
    },

    setDiskSpace: function(bfree, btotal) {
        human = function(val) {
          if (val > 1000000000)
            val = parseInt(val / 1000000000) + _('GB');
          if (val > 1000000)
            val = parseInt(val / 1000000) + _('MB');
          if (val > 1000)
            val = parseInt(val / 1000) + _('KB');
          return val;
        };
        text = _('Disk space:') + '&nbsp;<b>' + human(bfree) + '/' + human(btotal) + '</b>';
        var el = Ext.get(this.diskSpaceItem.tabEl).child('span.x-tab-diskspace', true);
        el.innerHTML = text;
        el.qtip = _('Free:') + ' ' + human(bfree) + ' ' + _('Total:') + ' ' + human(btotal);
    },

    onLoginCmdClicked: function(e) {
        window.location.href = this.login ? 'logout' : 'login';
    }

});

/**
 *
 */
//create application
tvheadend.app = function() {

    // public space
    return {
        // public methods
        init: function() {
            var header = new Ext.Panel({
                split: true,
                region: 'north',
                height: 45,
                boxMaxHeight: 45,
                boxMinHeight: 45,
                border: false,
                hidden: true,
                html: '<div id="header"><h1>' + _('Tvheadend Web-Panel') + '</h1></div>'
            });

            tvheadend.rootTabPanel = new tvheadend.RootTabPanel({
                region: 'center',
                activeTab: 0,
                items: [tvheadend.epg()]
            });

            var viewport = new Ext.Viewport({
                layout: 'border',
                items: [{
                        region: 'south',
                        contentEl: 'systemlog',
                        split: true,
                        autoScroll: true,
                        height: 150,
                        minSize: 100,
                        maxSize: 400,
                        collapsible: true,
                        collapsed: true,
                        title: _('System log'),
                        margins: '0 0 0 0',
                        tools: [{
                                id: 'gear',
                                qtip: _('Enable debug output'),
                                handler: function(event, toolEl, panel) {
                                    Ext.Ajax.request({
                                        url: 'comet/debug',
                                        params: {
                                            boxid: tvheadend.boxid
                                        }
                                    });
                                }
                            }]
                    }, tvheadend.rootTabPanel, header]
            });

            tvheadend.comet.on('accessUpdate', accessUpdate);

            tvheadend.comet.on('diskspaceUpdate', diskspaceUpdate);

            tvheadend.comet.on('setServerIpPort', setServerIpPort);

            tvheadend.comet.on('logmessage', function(m) {
                tvheadend.log(m.logtxt);
            });

            new tvheadend.cometPoller;

            Ext.QuickTips.init();
        }

    };
}(); // end of app
