/*
 * Status dialog
 */

tvheadend.service_mapper_status_panel = null;

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

    /* Form fields */
    var availCheck = new Ext.form.Checkbox({
        name: 'check_availability',
        fieldLabel: _('Check availability'),
        checked: false
    });
    var ftaCheck = new Ext.form.Checkbox({
        name: 'encrypted',
        fieldLabel: _('Include encrypted services'),
        checked: false
        // TODO: make dependent on CSA config
    });
    var mergeCheck = new Ext.form.Checkbox({
        name: 'merge_same_name',
        fieldLabel: _('Merge same name'),
        checked: false
    });
    var provtagCheck = new Ext.form.Checkbox({
        name: 'provider_tags',
        fieldLabel: _('Create provider tags'),
        checked: false
    });

    // TODO: provider list
    items = [availCheck, ftaCheck, mergeCheck, provtagCheck];

    /* Form */
    var undoBtn = new Ext.Button({
        text: 'Cancel',
        handler: function() {
            win.close();
        }
    });

    var saveBtn = new Ext.Button({
        text: 'Map',
        tooltip: _('Begin mapping'),
        handler: function() {
            p = null;
            if (select) {
                var r = select.getSelections();
                if (r.length > 0) {
                    var uuids = [];
                    for (var i = 0; i < r.length; i++)
                        uuids.push(r[i].id);
                    p = {uuids: Ext.encode(uuids)};
                }
            }


            panel.getForm().submit({
                url: 'api/service/mapper/start',
                waitMessage: _('Mapping services...'),
                params: p
            });

            win.hide();

            /* Dialog */
            win = new Ext.Window({
                title: _('Service Mapper Status'),
                iconCls: 'clone',
                layout: 'fit',
                autoWidth: true,
                autoHeight: true,
                plain: false,
                items: tvheadend.service_mapper_status_panel
                        // TODO: buttons
            });
            win.show();
        }
    });

    panel = new Ext.FormPanel({
        method: 'post',
        frame: true,
        border: true,
        bodyStyle: 'padding: 5px',
        labelAlign: 'left',
        labelWidth: 200,
        autoWidth: true,
        autoHeight: true,
        defaultType: 'textfield',
        buttonAlign: 'left',
        items: items,
        buttons: [undoBtn, saveBtn]
    });

    /* Create window */
    win = new Ext.Window({
        title: _('Map services'),
        iconCls: 'clone',
        layout: 'fit',
        autoWidth: true,
        autoHeight: true,
        plain: true,
        items: panel
    });

    win.show();
}
