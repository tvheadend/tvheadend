tvheadend.dynamic = true;
tvheadend.accessupdate = null;
tvheadend.capabilities = null;
tvheadend.admin = false;
tvheadend.dialog = null;
tvheadend.uilevel = 'expert';
tvheadend.uilevel_nochange = false;
tvheadend.quicktips = true;
tvheadend.wizard = null;

tvheadend.cookieProvider = new Ext.state.CookieProvider({
  // 7 days from now
  expires: new Date(new Date().getTime() + (1000 * 60 * 60 * 24 * 7))
});

/* State Provider */
Ext.state.Manager.setProvider(tvheadend.cookieProvider);

tvheadend.regexEscape = function(s) {
    return s.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
}

tvheadend.fromCSV = function(s) {
    var a = s.split(',');
    var r = [];
    for (var i in a) {
        var v = a[i];
        if (v[0] == '"' && v[v.length-1] == '"')
            r.push(v.substring(1, v.length - 1));
        else
            r.push(v);
    }
    return r;
}

/**
 * Change uilevel
 */
tvheadend.uilevel_match = function(target, current) {
    if (current !== 'expert') {
        if (current === 'advanced' && target === 'expert')
            return false;
        else if (current === 'basic' && target !== 'basic')
            return false;
    }
    return true;
}

/*
 * Select specific tab
 */
tvheadend.select_tab = function(id)
{
    var i = Ext.getCmp(id);
    var c = i ? i.ownerCt : null;
    while (c) {
        if ('activeTab' in c) {
            c.setActiveTab(i);
        }
        i = c;
        c = c.ownerCt;
    }
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
      Ext.getBody().mask(_('Loading, please wait...'), 'loading');
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
 * Replace one entry
 */

tvheadend.replace_entry = function(r, d) {
    if (!r) return;
    var dst = r.data;
    var src = d.params instanceof Array ? d.params : d;
    var lookup = src instanceof Array;
    r.store.fields.each(function (n) {
        if (lookup) {
            for (var i = 0; i < src.length; i++) {
                if (src[i].id == n.name) {
                    var v = src[i].value;
                    break;
                }
            }
        } else {
            var v = src[n.name];
        }
        var x = v;
        if (typeof v === 'undefined')
            x = typeof n.defaultValue === 'undefined' ? '' : n.defaultValue;
        dst[n.name] = n.convert(x, v);
    });
    r.json = src;
    r.commit();
}

/*
 * General capabilities
 */
Ext.Ajax.request({
    url: 'api/config/capabilities',
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
                iconCls: 'control_mute',
                tooltip: _('Toggle mute'),
                handler: function() {
                    var muted = videoPlayer.muteToggle();
                    slider.setDisabled(muted);
                    sliderLabel.setDisabled(muted);
                }
            },
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
  if ('freediskspace' in o && 'useddiskspace' in o && 'totaldiskspace' in o)
    tvheadend.rootTabPanel.setDiskSpace(o.freediskspace, o.useddiskspace, o.totaldiskspace);
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

    tvheadend.admin = o.admin == true;

    if (o.uilevel)
        tvheadend.uilevel = o.uilevel;
        
    tvheadend.quicktips = o.quicktips;

    if (o.uilevel_nochange)
        tvheadend.uilevel_nochange = true;

    if ('info_area' in o)
        tvheadend.rootTabPanel.setInfoArea(o.info_area);
    if ('username' in o)
        tvheadend.rootTabPanel.setLogin(o.username);
    if ('address' in o)
        tvheadend.rootTabPanel.setAddress(o.address);
    if ('freediskspace' in o && 'useddiskspace' in o && 'totaldiskspace' in o)
        tvheadend.rootTabPanel.setDiskSpace(o.freediskspace, o.useddiskspace, o.totaldiskspace);

    if ('cookie_expires' in o && o.cookie_expires > 0)
        tvheadend.cookieProvider.expires =
            new Date(new Date().getTime() + (1000 * 60 * 60 * 24 * o.cookie_expires));

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

        /* General */
        var general = new Ext.TabPanel({
            tabIndex: 0,
            activeTab: 0,
            autoScroll: true,
            title: _('General'),
            iconCls: 'general',
            items: []
        });

        tvheadend.baseconf(general);
        tvheadend.imgcacheconf(general);
        tvheadend.satipsrvconf(general);
        
        cp.add(general);

        /* Users */
        var users = new Ext.TabPanel({
            tabIndex: 1,
            activeTab: 0,
            autoScroll: true,
            title: _('Users'),
            iconCls: 'group',
            items: []
        });

        tvheadend.acleditor(users);
        tvheadend.passwdeditor(users);
        tvheadend.ipblockeditor(users);
        
        cp.add(users);

        /* DVB inputs, networks, muxes, services */
        var dvbin = new Ext.TabPanel({
            tabIndex: 2,
            activeTab: 0,
            autoScroll: true,
            title: _('DVB Inputs'),
            iconCls: 'hardware',
            items: []
        });

        if (tvheadend.capabilities.indexOf('tvadapters') !== -1)
            tvheadend.tvadapters(dvbin);
        tvheadend.networks(dvbin);
        tvheadend.muxes(dvbin);
        tvheadend.services(dvbin);
        tvheadend.mux_sched(dvbin);

        cp.add(dvbin);

        /* Channel / EPG */
        var chepg = new Ext.TabPanel({
            tabIndex: 3,
            activeTab: 0,
            autoScroll: true,
            title: _('Channel / EPG'),
            iconCls: 'television',
            items: []
        });
        tvheadend.channel_tab(chepg);
        tvheadend.cteditor(chepg);
        tvheadend.bouquet(chepg);
        tvheadend.epggrab_map(chepg);
        tvheadend.epggrab_base(chepg);
        tvheadend.epggrab_mod(chepg);

        cp.add(chepg);

        /* Stream Config */
        var stream = new Ext.TabPanel({
            tabIndex: 4,
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
            tabIndex: 5,
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
            tvheadend.caclient(cp, 6);

        /* Debug */
        tvheadend.tvhlog(cp, 7);

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

    if (o.wizard)
        tvheadend.wizard_start(o.wizard);
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
tvheadend.RootTabExtraComponent = Ext.extend(Ext.Component, {
   
    onRender1: function(tab, before) {
        if (!this.componentTpl) {
            var tt = new Ext.Template(
                '<li class="x-tab-extra-comp" id="{id}">',
                '<span class="x-tab-strip-extra-comp {iconCls} x-tab-strip-text">{text}</span></li>'
            );
            tt.disableFormats = true;
            tt.compile();
            tvheadend.RootTabExtraComponent.prototype.componentTpl = tt;
        }
        var p = tab.getTemplateArgs(this);
        var el = this.componentTpl.insertBefore(before, p);
        this.tabEl = Ext.get(el);
    }

});

tvheadend.RootTabExtraClickComponent = Ext.extend(Ext.Component, {
   
    onRender1: function(tab, before, click_cb) {
        if (!this.componentTpl) {
            var tt = new Ext.Template(
                '<li class="x-tab-extra-comp" id="{id}"><a href="#">',
                '<span class="x-tab-strip-extra-click-comp {iconCls} x-tab-strip-text">{text}</span></a></li>'
            );
            tt.disableFormats = true;
            tt.compile();
            tvheadend.RootTabExtraClickComponent.prototype.componentTpl = tt;
        }
        var p = tab.getTemplateArgs(this);
        var el = this.componentTpl.insertBefore(before, p);
        this.tabEl = Ext.get(el);
        this.tabEl.select('a').on('click', click_cb, tab, {preventDefault: true});
    }

});

tvheadend.RootTabPanel = Ext.extend(Ext.TabPanel, {

    extra: [],
    info_area: [],

    getComponent: function(comp) {
        for (var k in this.extra) {
            var comp2 = this.extra[k];
            if (comp === comp2.id || comp == comp2)
                return comp2;
        }
        return tvheadend.RootTabPanel.superclass.getComponent.call(this, comp);
    },

    setInfoArea: function(info_area) {
        this.info_area = tvheadend.fromCSV(info_area);
        this.on('beforetabchange', function(tp, p) {
            for (var k in this.extra)
                if (p == this.extra[k])
                    return false;
        });

        var before = this.strip.dom.childNodes[this.strip.dom.childNodes.length-1];

        /* Create extra components */
        for (var itm in this.info_area) {
            var nm = this.info_area[itm];
            if (!(nm in this.extra)) {
                this.extra[nm] = new tvheadend.RootTabExtraComponent();
                this.extra[nm].onRender1(this, before);
                if (nm == 'login') {
                    this.extra.loginCmd = new tvheadend.RootTabExtraClickComponent();
                    this.extra.loginCmd.onRender1(this, before, this.onLoginCmdClicked);
                }
            }
        }
        
        if (this.extra.time)
            window.setInterval(this.setTime, 1000);
    },

    setLogin: function(login) {
        if (!('login' in this.extra)) return;
        this.login = login;
        if (login) {
            text = _('Logged in as') + ' <b>' + login + '</b>';
            cmd = '(' + _('logout') + ')';
        } else {
            text = _('No verified access');
            cmd = '(' + _('login') + ')';
        }
        Ext.get(this.extra.login.tabEl).child('span.x-tab-strip-extra-comp', true).innerHTML = text;
        Ext.get(this.extra.loginCmd.tabEl).child('span.x-tab-strip-extra-click-comp', true).innerHTML = cmd;
    },

    setAddress: function(addr) {
        if ('login' in this.extra)
            Ext.get(this.extra.login.tabEl).child('span.x-tab-strip-extra-comp', true).qtip = addr;
    },

    setDiskSpace: function(bfree, bused, btotal) {
        if (!('storage' in this.extra)) return;
        human = function(val) {
          if (val > 1073741824)
            val = parseInt(val / 1073741824) + _('GiB');
          if (val > 1048576)
            val = parseInt(val / 1048576) + _('MiB');
          if (val > 1024)
            val = parseInt(val / 1024) + _('KiB');
          return val;
        };
        text = _('Storage space') + ':&nbsp;<b>' + human(bfree) + '/' + human(bused) + '/' + human(btotal) + '</b>';
        var el = Ext.get(this.extra.storage.tabEl).child('span.x-tab-strip-extra-comp', true);
        el.innerHTML = text;
        el.qtip = _('Free') + ': ' + human(bfree) + ' ' + _('Used by tvheadend') + ': ' + human(bused) + ' ' + _('Total') + ': ' + human(btotal);
    },

    setTime: function(stime) {
        var panel = tvheadend.rootTabPanel;
        if (!('time' in panel.extra)) return;
        var d = stime ? new Date(stime) : new Date();
        var el = Ext.get(panel.extra.time.tabEl).child('span.x-tab-strip-extra-comp', true);
        el.innerHTML = '<b>' + d.toLocaleTimeString() + '</b>';
        el.qtip = d.toLocaleString();
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
