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
    tvheadend.paneladd(panel, mpanel, index);
}

/*
 * Start mapping
 */
tvheadend.service_mapper = function(t, e, store, select)
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
        loadURL: 'api/service/mapper/load',
        saveURL: 'api/service/mapper/save',
        saveText: _('Map services'),
        alwaysDirty: true,
        noApply: true,
        modifyData: modify_data,
        postsave: function() {
            tvheadend.select_tab('service_mapper');
        },
        help: function() {
            new tvheadend.help(_('Map services'), 'config_mapper.html');
        }
    });
}
