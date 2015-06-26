tvheadend.timeshift = function(panel, index) {

    /* ****************************************************************
     * Data
     * ***************************************************************/

    var confreader = new Ext.data.JsonReader(
        {
            root: 'config'
        },
        [
            'timeshift_enabled', 'timeshift_ondemand',
            'timeshift_path',
            'timeshift_unlimited_period', 'timeshift_max_period',
            'timeshift_unlimited_size', 'timeshift_max_size',
            'timeshift_ram_size', 'timeshift_ram_only'
        ]
    );

    /* ****************************************************************
     * Fields
     * ***************************************************************/

    var timeshiftEnabled = new Ext.form.Checkbox({
        fieldLabel: _('Enabled'),
        name: 'timeshift_enabled',
        width: 300
    });

    var timeshiftOndemand = new Ext.form.Checkbox({
        fieldLabel: _('On-Demand'),
        name: 'timeshift_ondemand',
        width: 300
    });

    var timeshiftPath = new Ext.form.TextField({
        fieldLabel: _('Storage Path'),
        name: 'timeshift_path',
        allowBlank: true,
        width: 300
    });

    var timeshiftMaxPeriod = new Ext.form.NumberField({
        fieldLabel: _('Maximum Period (mins)'),
        name: 'timeshift_max_period',
        allowBlank: false,
        width: 300
    });

    var timeshiftUnlPeriod = new Ext.form.Checkbox({
        fieldLabel: _('Unlimited time'),
        name: 'timeshift_unlimited_period',
        width: 300
    });

    var timeshiftMaxSize = new Ext.form.NumberField({
        fieldLabel: _('Maximum Size (MB)'),
        name: 'timeshift_max_size',
        allowBlank: false,
        width: 300
    });

    var timeshiftRamSize = new Ext.form.NumberField({
        fieldLabel: _('Maximum RAM Size (MB)'),
        name: 'timeshift_ram_size',
        allowBlank: false,
        width: 250
    });

    var timeshiftUnlSize = new Ext.form.Checkbox({
        fieldLabel: _('Unlimited size'),
        name: 'timeshift_unlimited_size',
        width: 300
    });

    var timeshiftRamOnly = new Ext.form.Checkbox({
        fieldLabel: _('Use only RAM'),
        name: 'timeshift_ram_only',
        width: 300
    });

    /* ****************************************************************
     * Events
     * ***************************************************************/

    timeshiftUnlPeriod.on('check', function(e, c){
        timeshiftMaxPeriod.setDisabled(c);
    });

    timeshiftUnlSize.on('check', function(e, c){
        timeshiftMaxSize.setDisabled(c || timeshiftMaxSize.getValue());
    });

    timeshiftRamOnly.on('check', function(e, c){
        timeshiftMaxSize.setDisabled(c || timeshiftUnlSize.getValue());
    });

    /* ****************************************************************
     * Form
     * ***************************************************************/

    var saveButton = new Ext.Button({
        text: _("Save configuration"),
        tooltip: _('Save changes made to configuration below'),
        iconCls: 'save',
        handler: saveChanges
    });

    var helpButton = new Ext.Button({
        text: _('Help'),
        iconCls: 'help',
        handler: function() {
            new tvheadend.help(_('Timeshift Configuration'), 'config_timeshift.html');
        }
    });

    var timeshiftPanelA = new Ext.form.FieldSet({
        width: 500,
        autoHeight: true,
        border: false,
        items : [timeshiftMaxPeriod, timeshiftMaxSize, timeshiftRamSize]
    });

    var timeshiftPanelB = new Ext.form.FieldSet({
        width: 200,
        autoHeight: true,
        border: false,
        items : [timeshiftUnlPeriod, timeshiftUnlSize, timeshiftRamOnly]
    });

    var timeshiftPanel = new Ext.form.FieldSet({
        title: _('Timeshift Options'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [
            timeshiftEnabled, 
            timeshiftOndemand, 
            timeshiftPath,
            {
                layout: 'column', 
                border: false,
                items: [timeshiftPanelA, timeshiftPanelB]
            }	
        ]
    });

    var confpanel = new Ext.form.FormPanel({
        title : _('Timeshift'),
        iconCls : 'timeshift',
        border : false,
        bodyStyle : 'padding:15px',
        labelAlign : 'left',
        labelWidth : 150,
        waitMsgTarget : true,
        reader : confreader,
        layout : 'form',
        defaultType : 'textfield',
        autoHeight : true,
        animCollapse : true,
    	items : [timeshiftPanel],
        tbar : [saveButton, '->', helpButton]
    });

    /* ****************************************************************
     * Load/Save
     * ***************************************************************/

    confpanel.on('render', function() {
        confpanel.getForm().load({
            url: 'timeshift',
            params: {
                'op': 'loadSettings'
            },
            success: function() {
                confpanel.enable();
                timeshiftMaxPeriod.setDisabled(timeshiftUnlPeriod.getValue());
                timeshiftMaxSize.setDisabled(timeshiftUnlSize.getValue() || timeshiftRamOnly.getValue());
            }
        });
    });

    function saveChanges() {
        confpanel.getForm().submit({
            url: 'timeshift',
            params: {
                op: 'saveSettings'
            },
            waitMsg: _('Saving Data...'),
            success: function(form, action) {
            },
            failure: function(form, action) {
                Ext.Msg.alert(_('Save failed'), action.result.errormsg);
            }
        });
    }

    tvheadend.paneladd(panel, confpanel, index);
};
