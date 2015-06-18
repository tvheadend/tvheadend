tvheadend.epggrabChannels = new Ext.data.JsonStore({
    root: 'entries',
    url: 'epggrab',
    baseParams: {
        op: 'channelList'
    },
    fields: ['id', 'mod', 'name', 'icon', 'number', 'channel', 'mod-id', 'mod-name']
});

tvheadend.epggrab = function(panel, index) {

    /* ****************************************************************
     * Data
     * ***************************************************************/

    /*
     * Module lists (I'm sure there is a better way!)
     */
    var EPGGRAB_MODULE_INTERNAL = "internal";
    var EPGGRAB_MODULE_EXTERNAL = "external";
    var EPGGRAB_MODULE_OTA = "ota";

    var moduleStore = new Ext.data.JsonStore({
        root: 'entries',
        url: 'epggrab',
        baseParams: {
            op: 'moduleList'
        },
        autoLoad: true,
        fields: ['id', 'name', 'path', 'type', 'enabled']
    });
    var internalModuleStore = new Ext.data.Store({
        recordType: moduleStore.recordType
    });
    var externalModuleStore = new Ext.data.Store({
        recordType: moduleStore.recordType
    });
    var otaModuleStore = new Ext.data.Store({
        recordType: moduleStore.recordType
    });
    moduleStore.on('load', function() {
        moduleStore.filterBy(function(r) {
            return r.get('type') === EPGGRAB_MODULE_INTERNAL;
        });
        r = new internalModuleStore.recordType({
            id: '',
            name: _('Disabled')
        });
        internalModuleStore.add(r);
        moduleStore.each(function(r) {
            internalModuleStore.add(r.copy());
        });
        moduleStore.filterBy(function(r) {
            return r.get('type') === EPGGRAB_MODULE_EXTERNAL;
        });
        moduleStore.each(function(r) {
            externalModuleStore.add(r.copy());
        });
        moduleStore.filterBy(function(r) {
            return r.get('type') === EPGGRAB_MODULE_OTA;
        });
        moduleStore.each(function(r) {
            otaModuleStore.add(r.copy());
        });
        moduleStore.filterBy(function(r) {
            return r.get('type') !== EPGGRAB_MODULE_INTERNAL;
        });
    });

    /* Enable module in one of the stores (will auto update primary) */
    function moduleSelect(r, e) {
        r.set('enabled', e);
        t = moduleStore.getById(r.id);
        if (t)
            t.set('enabled', e);
    }

    /*
     * Basic Config
     */

    var confreader = new Ext.data.JsonReader({
        root: 'epggrabSettings'
    }, ['module', 'cron', 'channel_rename', 'channel_renumber',
        'channel_reicon', 'epgdb_periodicsave',
        'ota_cron', 'ota_timeout', 'ota_initial']);

    /* ****************************************************************
     * Basic Fields
     * ***************************************************************/

    /*
     * Cron setup
     */
    var internalCron = new Ext.form.TextArea({
        fieldLabel: _('Cron multi-line'),
        name: 'cron',
        width: 350
    });

    /*
     * Module selector
     */
    var internalModule = new Ext.form.ComboBox({
        fieldLabel: _('Module'),
        hiddenName: 'module',
        width: 350,
        valueField: 'id',
        displayField: 'name',
        forceSelection: true,
        editable: false,
        mode: 'local',
        triggerAction: 'all',
        store: internalModuleStore
    });

    /*
     * Channel handling
     */
    var channelRename = new Ext.form.Checkbox({
        name: 'channel_rename',
        fieldLabel: _('Update channel name')
    });

    var channelRenumber = new Ext.form.Checkbox({
        name: 'channel_renumber',
        fieldLabel: _('Update channel number')
    });

    var channelReicon = new Ext.form.Checkbox({
        name: 'channel_reicon',
        fieldLabel: _('Update channel icon')
    });

    var epgPeriodicSave = new Ext.form.NumberField({
        width: 30,
        allowNegative: false,
        allowDecimals: false,
        minValue: 0,
        maxValue: 24,
        value: 0,
        fieldLabel: _('Periodic save EPG to disk'),
        name: 'epgdb_periodicsave'
    });

    /*
     * Simple fields
     */
    var simplePanel = new Ext.form.FieldSet({
        title: _('General Config'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        items: [channelRename, channelRenumber, channelReicon, epgPeriodicSave]
    });

    /*
     * Internal grabber
     */
    var internalPanel = new Ext.form.FieldSet({
        title: _('Internal Grabber'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        items: [internalCron, internalModule]
    });

    /* ****************************************************************
     * Advanced Fields
     * ***************************************************************/

    /*
     * External modules
     */
    var externalSelectionModel = new Ext.grid.CheckboxSelectionModel({
        singleSelect: false,
        listeners: {
            'rowselect': function(s, ri, r) {
                moduleSelect(r, 1);
            },
            'rowdeselect': function(s, ri, r) {
                moduleSelect(r, 0);
            }
        }
    });

    var externalColumnModel = new Ext.grid.ColumnModel([externalSelectionModel,
        {
            header: _('Module'),
            dataIndex: 'name',
            width: 200,
            sortable: false
        }, {
            header: 'Path',
            dataIndex: 'path',
            width: 300,
            sortable: false
        }]);

    var externalGrid = new Ext.grid.EditorGridPanel({
        store: externalModuleStore,
        cm: externalColumnModel,
        sm: externalSelectionModel,
        width: 600,
        height: 150,
        frame: false,
        viewConfig: {
            forceFit: true
        },
        iconCls: 'icon-grid'
    });

    var externalPanel = new Ext.form.FieldSet({
        title: _('External Interfaces'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        items: [externalGrid]
    });

    /*
     * OTA modules
     */

    var otaSelectionModel = new Ext.grid.CheckboxSelectionModel({
        singleSelect: false,
        listeners: {
            'rowselect': function(s, ri, r) {
                moduleSelect(r, 1);
            },
            'rowdeselect': function(s, ri, r) {
                moduleSelect(r, 0);
            }
        }
    });

    var otaColumnModel = new Ext.grid.ColumnModel([otaSelectionModel, {
            header: _('Module'),
            dataIndex: 'name',
            width: 200,
            sortable: false
        }]);

    var otaGrid = new Ext.grid.EditorGridPanel({
        store: otaModuleStore,
        cm: otaColumnModel,
        sm: otaSelectionModel,
        width: 600,
        height: 150,
        frame: false,
        viewConfig: {
            forceFit: true
        },
        iconCls: 'icon-grid'
    });

    var otaInitial = new Ext.form.Checkbox({
        name: 'ota_initial',
        fieldLabel: _('Force initial EPG scan at startup')
    });

    var otaCron = new Ext.form.TextArea({
        fieldLabel: _('Over-the-air Cron multi-line'),
        name: 'ota_cron',
        width: 350
    });

    var otaTimeout = new Ext.form.NumberField({
        width: 40,
        allowNegative: false,
        allowDecimals: false,
        minValue: 30,
        maxValue: 7200,
        value: 600,
        fieldLabel: _('EPG scan timeout in seconds (30-7200)'),
        name: 'ota_timeout'
    });

    var otaPanel = new Ext.form.FieldSet({
        title: _('Over-the-air Grabbers'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        items: [otaInitial, otaCron, otaTimeout, otaGrid]
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

    var otaepgButton = new Ext.Button({
        text: _("Trigger OTA EPG Grabber"),
        tooltip: _('Tune over-the-air EPG muxes to grab new events now'),
        iconCls: 'find',
        handler: otaepgTrigger
    });

    var helpButton = new Ext.Button({
        text: _('Help'),
	iconCls: 'help',
        handler: function() {
            new tvheadend.help(_('EPG Grab Configuration'), 'config_epggrab.html');
        }
    });

    var confpanel = new Ext.FormPanel({
        title: _('EPG Grabber'),
        iconCls: 'xml',
        border: false,
        bodyStyle: 'padding:15px',
        labelAlign: 'left',
        labelWidth: 150,
        waitMsgTarget: true,
        reader: confreader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: [simplePanel, internalPanel, otaPanel, externalPanel],
        tbar: [saveButton, otaepgButton, '->', helpButton]
    });

    /* ****************************************************************
     * Load/Save
     * ***************************************************************/

    /* HACK: get display working */
    externalGrid.on('render', function() {
        delay = new Ext.util.DelayedTask(function() {
            rows = [];
            externalModuleStore.each(function(r) {
                if (r.get('enabled'))
                    rows.push(r);
            });
            externalSelectionModel.selectRecords(rows);
        });
        delay.delay(100);
    });
    otaGrid.on('render', function() {
        delay = new Ext.util.DelayedTask(function() {
            rows = [];
            otaModuleStore.each(function(r) {
                if (r.get('enabled'))
                    rows.push(r);
            });
            otaSelectionModel.selectRecords(rows);
        });
        delay.delay(100);
    });

    confpanel.on('render', function() {

        /* Hack to get display working */
        delay = new Ext.util.DelayedTask(function() {
            simplePanel.doLayout(false);
            internalPanel.doLayout(false);
            externalPanel.doLayout(false);
            otaPanel.doLayout(false);
        });
        delay.delay(100);

        confpanel.getForm().load({
            url: 'epggrab',
            params: {
                op: 'loadSettings'
            },
            success: function(form, action) {
                confpanel.enable();
            }
        });
    });

    function saveChanges() {
        mods = [];
        moduleStore.each(function(r) {
            mods.push({
                id: r.get('id'),
                enabled: r.get('enabled') ? 1 : 0
            });
        });
        mods = Ext.util.JSON.encode(mods);
        confpanel.getForm().submit({
            url: 'epggrab',
            params: {
                op: 'saveSettings',
                external: mods
            },
            waitMsg: _('Saving Data...'),
            success: function(form, action) {
                externalModuleStore.commitChanges();
            },
            failure: function(form, action) {
                Ext.Msg.alert('Save failed', action.result.errormsg);
            }
        });
    }

    function otaepgTrigger() {
        Ext.Ajax.request({
            url: 'epggrab',
            params: {
                op: 'otaepgTrigger',
                after: 1
            },
            waitMsg: _('Triggering...'),
            failure: function(response, options) {
                Ext.Msg.alert('Trigger failed', response.statusText);
            }
        });
    }

    tvheadend.paneladd(panel, confpanel, index);
};
