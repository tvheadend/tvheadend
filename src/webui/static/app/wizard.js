/*
 * Wizard
 */

tvheadend.wizard_start = function(page) {

    var w = null;
    var tabMapping = {
        hello: 'access_entry',
        network: 'mpegts_network',
        input: 'tvadapters',
        status: 'status_streams',
        mapping: 'channels',
    }

    function cancel(conf) {
        tvheadend.Ajax({
            url: 'api/wizard/cancel'
        });
        tvheadend.wizard = null;
        if (conf.win)
            conf.win.close();
    }

    function getparam(data, prefix) {
        var m = data.params;
        var l = prefix.length;
        for (var i = 0; i < m.length; i++) {
            var id = m[i].id;
            if (id.substring(0, l) === prefix)
                return id.substring(l, 32);
        }
        return null;
    }

    function getvalue(data, prefix) {
        var m = data.params;
        var l = prefix.length;
        for (var i = 0; i < m.length; i++) {
            var id = m[i].id;
            if (id === prefix)
                return m[i].value;
        }
        return null;
    }

    function newpage(conf, prefix) {
        var p = getparam(conf.fullData, prefix);
        if (p)
            tvheadend.wizard_start(p);
        else
            Ext.MessageBox.alert(String.format(_('Wizard - page "{0}" not found'), prefix));
    }

    function buildbtn(conf, buttons) {
        if (!getparam(conf.fullData, 'page_prev_'))
            return;
        var prevBtn = new Ext.Button({
            text: _('Previous'),
            iconCls: 'previous',
            handler: function() {
                newpage(conf, 'page_prev_');
            },
        });
        buttons.splice(0, 0, prevBtn);
    }

    function pbuild(conf, panel) {
        var data = conf.fullData;
        var icon = getvalue(data, 'icon');
        var text = getvalue(data, 'description');
        var c = '';
        if (icon)
            c += '<img class="x-wizard-icon" src="' + icon + '"/>';
        if (text) {
            var a = text.split('\n');
            text = '';
            for (var i = 0; i < a.length; i++)
                text += '<p>' + a[i] + '</p>';
        }
        c += '<div class="x-wizard-description">' + text + '</div>';
        var p = new Ext.Panel({
            width: 570,
            html: c
        });
        panel.insert(0, p);
    }
    
    function build(d) {
        d = json_decode(d);
        var m = d[0];
        var last = getparam(m, 'page_next_') === null;
        tvheadend.idnode_editor_win('basic', m, {
            build: pbuild,
            fullData: m,
            url: 'api/wizard/' + page,
            winTitle: m.caption,
            iconCls: 'wizard',
            comet: m.events,
            noApply: true,
            noUIlevel: true,
            postsave: function(conf) {
                if (!last)
                    newpage(conf, 'page_next_');
                else
                    cancel(conf);
            },
            buildbtn: buildbtn,
            labelWidth: 250,
            saveText: last ? _('Finish') : _('Save & Next'),
            saveIconCls: last ? 'finish' : 'next',
            cancel: cancel,
            uilevel: 'expert',
            help: function() {
                new tvheadend.help(_('Wizard - initial configuration and tutorial'), 'config_wizard.html');
            }
        });
    }

    tvheadend.wizard = page;

    if (page in tabMapping) {
        var i = Ext.getCmp(tabMapping[page]);
        var c = i ? i.ownerCt : null;
        while (c) {
            if ('activeTab' in c)
                c.setActiveTab(i);
            i = c;
            c = c.ownerCt;
        }
    }

    tvheadend.Ajax({
        url: 'api/wizard/' + page + '/load',
        params: {
            meta: 1
        },
        success: build,
        failure: function(response, options) {
            Ext.MessageBox.alert(_('Unable to obtain wizard page!'), response.statusText);
        }
    });

};
