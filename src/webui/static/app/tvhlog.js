tvheadend.tvhlog = function() {
    /*
     * Basic Config
     */
    var confreader = new Ext.data.JsonReader({
        root: 'config'
    }, ['tvhlog_path', 'tvhlog_dbg_syslog', 'tvhlog_trace_on',
        'tvhlog_debug', 'tvhlog_trace']);

    /* ****************************************************************
     * Form Fields
     * ***************************************************************/

    var tvhlogLogPath = new Ext.form.TextField({
        fieldLabel: 'Debug log path',
        name: 'tvhlog_path',
        allowBlank: true,
        width: 400
    });

    var tvhlogToSyslog = new Ext.form.Checkbox({
        name: 'tvhlog_dbg_syslog',
        fieldLabel: 'Debug to syslog'
    });

    var tvhlogTraceOn = new Ext.form.Checkbox({
        name: 'tvhlog_trace_on',
        fieldLabel: 'Debug trace (low-level stuff)'
    });

    var tvhlogDebugSubsys = new Ext.form.TextField({
        fieldLabel: 'Debug subsystems',
        name: 'tvhlog_debug',
        allowBlank: true,
        width: 400
    });

    var tvhlogTraceSubsys = new Ext.form.TextField({
        fieldLabel: 'Trace subsystems',
        name: 'tvhlog_trace',
        allowBlank: true,
        width: 400
    });

    /* ****************************************************************
     * Form
     * ***************************************************************/

    var saveButton = new Ext.Button({
        text: "Save configuration",
        tooltip: 'Save changes made to configuration below',
        iconCls: 'save',
        handler: saveChanges
    });

    var helpButton = new Ext.Button({
        text: 'Help',
        handler: function() {
            new tvheadend.help('Debug Configuration', 'config_tvhlog.html');
        }
    });

    var DebuggingPanel = new Ext.form.FieldSet({
        title: 'Debugging Options',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse : true,
        items: [tvhlogLogPath, tvhlogToSyslog,
            tvhlogTraceOn, tvhlogDebugSubsys, tvhlogTraceSubsys]
    });

    var confpanel = new Ext.form.FormPanel({
        title: 'Debugging',
        iconCls: 'wrench',
        border: false,
        bodyStyle: 'padding:15px',
        labelAlign: 'left',
        labelWidth: 200,
        waitMsgTarget: true,
        reader: confreader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: [DebuggingPanel],
        tbar: [saveButton, '->', helpButton]
    });

    /* ****************************************************************
     * Load/Save
     * ***************************************************************/

    confpanel.on('render', function() {
        confpanel.getForm().load({
            url: 'tvhlog',
            params: {
                op: 'loadSettings'
            },
            success: function(form, action) {
                confpanel.enable();
            }
        });
    });

    function saveChanges() {
        confpanel.getForm().submit({
            url: 'tvhlog',
            params: {
                op: 'saveSettings'
            },
            waitMsg: 'Saving Data...',
            failure: function(form, action) {
                Ext.Msg.alert('Save failed', action.result.errormsg);
            }
        });
    }

    return confpanel;
};
