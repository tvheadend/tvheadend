/*
 * Wizard
 */

tvheadend.wizard_delayed_activation = null;

tvheadend.wizard_start = function(page) {

    var tabMapping = {
        hello: 'base_config',
        login: 'access_entry',
        network: 'tvadapters',
        muxes: 'mpegts_network',
        status: 'status_streams',
        mapping: 'services',
        channels: 'channels'
    }

    function cancel(conf, noajax) {
        if (!noajax) {
            tvheadend.Ajax({
                url: 'api/wizard/cancel'
            });
        }
        tvheadend.wizard = null;
        if (conf.win)
            conf.win.close();
        tvheadend.wizard_delayed_activation.cancel();
        tvheadend.wizard_delayed_activation = null;
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

    function progressupdate() {
        var conf = this;
        Ext.Ajax.request({
            url: 'api/wizard/' + page + '/progress',
            success: function(d) {
                d = json_decode(d);
                var t = conf.progress.initialConfig.text;
                var i = d['progress'];
                conf.progress.updateProgress(i, t + ' ' + Math.round(100*i) + '%');
                conf.progress_task.delay(1000, progressupdate, conf);
                for (var key in d)
                    if (key !== 'progress') {
                        var f = conf.form.findField(key);
                        if (f)
                             f.setValue(d[key]);
                    }
            },
        });
    }

    function pbuild(conf, panel) {
        var data = conf.fullData;
        var icon = getvalue(data, 'icon');
        var text = getvalue(data, 'description');
        var progress = getvalue(data, 'progress');
        conf.form = panel.getForm();
        var c = '';
        if (icon)
            c += '<img class="x-wizard-icon" src="' + icon + '"/>';
        if (text)
            text = micromarkdown.parse(text);
        c += '<div class="x-wizard-description">' + text + '</div>';
        var p = new Ext.Panel({
            width: 570,
            html: c
        });
        panel.insert(0, p);
        if (progress) {
            conf.progress = new Ext.ProgressBar({
                text: progress,
                style: 'margin: 5px 0 15px;'
            });
            panel.insert(1, conf.progress);
            conf.progress_task = new Ext.util.DelayedTask(progressupdate, conf);
            conf.progress_task.delay(1000);
        }
    }
    
    function build(d) {
        d = json_decode(d);
        var m = d[0];
        var last = getparam(m, 'page_next_') === null;
        tvheadend.idnode_editor_win('basic', {
            build: pbuild,
            fullData: m,
            saveURL: 'api/wizard/' + page + '/save',
            winTitle: m.caption,
            iconCls: 'wizard',
            comet: m.events,
            noApply: true,
            noUIlevel: true,
            alwaysDirty: last,
            presave: function(conf, data) {
                if (last) {
                    tvheadend.Ajax({
                        url: 'api/wizard/cancel'
                    });
                }
            },
            postsave: function(conf, data) {
                if (data) {
                    if (('ui_lang' in data) && data['ui_lang'] != tvh_locale_lang) {
                        window.location.reload();
                        return;
                    }
                }
                if (!last) {
                    newpage(conf, 'page_next_');
                } else {
                    cancel(conf, 1);
                    window.location.reload();
                }
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

    function activate_tab() {
        if (page in tabMapping)
            tvheadend.select_tab(tabMapping[page]);
    }

    tvheadend.wizard = page;

    if (tvheadend.wizard_delayed_activation == null)
        tvheadend.wizard_delayed_activation = new Ext.util.DelayedTask();
    tvheadend.wizard_delayed_activation.delay(1000, activate_tab);

    tvheadend.Ajax({
        url: 'api/wizard/' + page + '/load',
        params: {
            meta: 1
        },
        success: build,
        failure: function(response, options) {
            Ext.MessageBox.alert(_('Unable to obtain wizard page!'), response.statusText);
            tvheadend.wizard_delayed_activation.cancel();
            tvheadend.wizard_delayed_activation = null;
        }
    });

};
