/*
 * Status dialog
 */

tvheadend.service_mapper_status = function(panel, index)
{
    /* Fields */
    var ok = new Ext.form.Label({
        fieldLabel: _('Mapped'),
        text: '0'
    });
    var fail = new Ext.form.Label({
        fieldLabel: _('Failed'),
        text: '0'
    });
    var ignore = new Ext.form.Label({
        fieldLabel: _('Ignored'),
        text: '0'
    });
    var active = new Ext.form.Label({
        width: 200,
        fieldLabel: _('Active'),
        text: ''
    });
    var prog = new Ext.ProgressBar({
        text: '0 / 0'
    });

    /* Panel */
    var mpanel = new Ext.FormPanel({
        id: 'service_mapper',
        method: 'get',
        title: _('Service Mapper'),
        iconCls: 'serviceMapper',
        frame: true,
        border: true,
        bodyStyle: 'padding: 5px',
        labelAlign: 'left',
        labelWidth: 200,
        width: 400,
        autoHeight: true,
        defaultType: 'textfield',
        buttonAlign: 'left',
        items: [ok, ignore, fail, active, prog]
    });

    /* Top panel */
    var tpanel = new Ext.Panel({
        title: _('Service Mapper'),
        iconCls: 'serviceMapper',
        layout: 'fit',
        tbar: ['->', {
             text: _('Help'),
             tooltip: _('View help docs.'),
             iconCls: 'help',
             handler: function() {
                 new tvheadend.mdhelp('status_service_mapper')
             }
        }],
        items: [mpanel]
    });

    /* Comet */
    tvheadend.comet.on('servicemapper', function(m) {
        var n = m.ok + m.ignore + m.fail;
        ok.setText('' + m.ok);
        ignore.setText('' + m.ignore);
        fail.setText('' + m.fail);
        active.setText('');
        prog.updateProgress(n / m.total, '' + n + ' / ' + m.total);

        if (m.active) {
            Ext.Ajax.request({
                url: 'api/idnode/load',
                params: {
                    uuid: m.active
                },
                success: function(d) {
                    d = Ext.util.JSON.decode(d.responseText);
                    try {
                        active.setText(d.entries[0].text);
                    } catch (e) {
                    }
                }
            });
        }
    });

    tvheadend.service_mapper_status_panel = mpanel;
    tvheadend.paneladd(panel, tpanel, index);
}

/*
 * Start mapping
 */
tvheadend.service_mapper_sel = function(t, e, store, select)
{
    var panel = null;
    var win = null;

    function modify_data(conf, d) {
        for (var i = 0; i < d.params.length; i++)
           if (d.params[i].id === 'services')
             break;
        if (select && i < d.params.length) {
            var r = select.getSelections();
            if (r.length > 0) {
                var uuids = [];
                for (var j = 0; j < r.length; j++)
                    uuids.push(r[j].id);
                d.params[i].value = uuids;
            }
        }
    }

    tvheadend.idnode_editor_win(tvheadend.uilevel, {
        winTitle: _('Map services to channels'),
        loadURL: 'api/service/mapper/load',
        saveURL: 'api/service/mapper/save',
        saveText: _('Map services'),
        alwaysDirty: true,
        noApply: true,
        modifyData: select ? modify_data : null,
        postsave: function() {
            tvheadend.select_tab('service_mapper');
        }
    });
}

tvheadend.service_mapper0 = function(all)
{
    tvheadend.idnode_editor_win(tvheadend.uilevel, {
        winTitle: _('Map services to channels'),
        loadURL: 'api/service/mapper/load',
        saveURL: 'api/service/mapper/save',
        saveText: _('Map services'),
        alwaysDirty: true,
        noApply: true,
        beforeShow: all ? function(panel, conf) {
            var form = panel.getForm();
            var services = form.findField('services');
            services.on('afterrender', function() {
                services.selectAll();
            });
            services.store.on('load', function() {
                services.selectAll();
            });
        } : null,
        postsave: function() {
            tvheadend.select_tab('service_mapper');
        }
    });
}

tvheadend.service_mapper_all = function()
{
    tvheadend.service_mapper0(1);
}

tvheadend.service_mapper_none = function()
{
    tvheadend.service_mapper0(0);
}
