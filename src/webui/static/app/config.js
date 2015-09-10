// Store: config languages
tvheadend.languages = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['identifier', 'name'],
    id: 'identifier',
    url: 'languages',
    baseParams: {
        op: 'list'
    }
});

// Store: all languages
tvheadend.config_languages = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['identifier', 'name'],
    id: 'identifier',
    url: 'languages',
    baseParams: {
        op: 'config'
    }
});

tvheadend.languages.setDefaultSort('name', 'ASC');

tvheadend.comet.on('config', function(m) {
    if (m.reload != null) {
        tvheadend.languages.reload();
        tvheadend.config_languages.reload();
    }
});

tvheadend.baseconf = function(panel, index) {

    /*
    * Base Config
    */

    var confreader = new Ext.data.JsonReader({
        root: 'config'
    },
    [
        'server_name', 'muxconfpath', 'language',
        'tvhtime_update_enabled', 'tvhtime_ntp_enabled',
        'tvhtime_tolerance',
        'prefer_picon', 'chiconpath', 'piconpath',
    ]);

    /* ****************************************************************
    * Form Fields
    * ***************************************************************/

    /*
    * DVB path
    */

    var serverName = new Ext.form.TextField({
        fieldLabel: _('Tvheadend server name'),
        name: 'server_name',
        allowBlank: true,
        width: 400
    });

    var serverWrap = new Ext.form.FieldSet({
        title: _('Server'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [ serverName ]
    });

    var dvbscanPath = new Ext.form.TextField({
        fieldLabel: _('DVB scan files path'),
        name: 'muxconfpath',
        allowBlank: true,
        width: 400
    });

    var dvbscanWrap = new Ext.form.FieldSet({
        title: _('DVB Scan Files'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [ dvbscanPath ]
    });

    /*
    * Language
    */

    var language = new Ext.ux.ItemSelector({
        name: 'language',
        fromStore: tvheadend.languages,
        toStore: tvheadend.config_languages,
        fieldLabel: _('Default Language(s)'),
        dataFields: ['identifier', 'name'],
        msWidth: 190,
        msHeight: 150,
        valueField: 'identifier',
        displayField: 'name',
        imagePath: 'static/multiselect/resources',
        toLegend: _('Selected'),
        fromLegend: _('Available')
    });

    var languageWrap = new Ext.form.FieldSet({
        title: _('Language Settings'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [ language ]
    });

    /*
    * Time/Date
    */

    var tvhtimeUpdateEnabled = new Ext.ux.form.XCheckbox({
        name: 'tvhtime_update_enabled',
        fieldLabel: _('Update time')
    });

    var tvhtimeNtpEnabled = new Ext.ux.form.XCheckbox({
        name: 'tvhtime_ntp_enabled',
        fieldLabel: _('Enable NTP driver')
    });

    var tvhtimeTolerance = new Ext.form.NumberField({
        name: 'tvhtime_tolerance',
        fieldLabel: _('Update tolerance (ms)')
    });

    var tvhtimePanel = new Ext.form.FieldSet({
        title: _('Time Update'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [tvhtimeUpdateEnabled, tvhtimeNtpEnabled, tvhtimeTolerance]
    });

    /*
    * Picons
    */

    var preferPicon = new Ext.ux.form.XCheckbox({
        name: 'prefer_picon',
        fieldLabel: _('Prefer picons over channel name')
    });

    var chiconPath = new Ext.form.TextField({
        name: 'chiconpath',
        fieldLabel: _('Channel icon path (see Help)'),
        width: 400
    });

    var piconPath = new Ext.form.TextField({
        name: 'piconpath',
        fieldLabel: _('Picon path (see Help)'),
        width: 400
    });

    var piconPanel = new Ext.form.FieldSet({
        title: _('Picon'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [preferPicon, chiconPath, piconPath]
    });

    /*
    * SAT>IP server
    */


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
            new tvheadend.help(_('General Configuration'), 'config_general.html');
        }
    });

    var _items = [serverWrap, languageWrap, dvbscanWrap, tvhtimePanel, piconPanel];

    var confpanel = new Ext.form.FormPanel({
        labelAlign: 'left',
        labelWidth: 200,
        border: false,
        waitMsgTarget: true,
        reader: confreader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: _items
    });

    var mpanel = new Ext.Panel({
        title: _('Base'),
        iconCls: 'baseconf',
        border: false,
        autoScroll: true,
        bodyStyle: 'padding:15px',
        layout: 'form',
        items: [confpanel],
        tbar: [saveButton, '->', helpButton]
    });

    tvheadend.paneladd(panel, mpanel, index);

    /* ****************************************************************
    * Load/Save
    * ***************************************************************/

    confpanel.on('render', function() {
        confpanel.getForm().load({
            url: 'config',
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
            url: 'config',
            params: {
                op: 'saveSettings'
            },
            waitMsg: _('Saving Data...'),
            failure: function(form, action) {
                Ext.Msg.alert(_('Save failed'), action.result.errormsg);
            }
        });
    }
};

/*
 * Imagecache configuration
 */

tvheadend.imgcacheconf = function(panel, index) {

    if (tvheadend.capabilities.indexOf('imagecache') === -1)
        return;

    var cleanButton = {
        name: 'clean',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Clean image cache on storage'),
                iconCls: 'clean',
                text: _('Clean image (icon) cache')
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
               url: 'api/imagecache/config/clean',
               params: { clean: 1 },
            });
        }
    };

    var triggerButton = {
        name: 'trigger',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Re-fetch images'),
                iconCls: 'trigger',
                text: _('Re-fetch images'),
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
               url: 'api/imagecache/config/trigger',
               params: { trigger: 1 },
            });
        }
    };

    tvheadend.idnode_simple(panel, {
        url: 'api/imagecache/config',
        title: _('Image cache'),
        iconCls: 'imgcacheconf',
        tabIndex: index,
        comet: 'imagecache',
        tbar: [cleanButton, triggerButton],
        help: function() {
            new tvheadend.help(_('General Configuration'), 'config_general.html');
        }
    });

};

/*
 * SAT>IP server configuration
 */

tvheadend.satipsrvconf = function(panel, index) {

    if (tvheadend.capabilities.indexOf('satip_server') === -1)
        return;

    var discoverButton = {
        name: 'discover',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Look for new SAT>IP servers'),
                iconCls: 'find',
                text: _('Discover SAT>IP servers')
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
                url: 'api/hardware/satip/discover',
                params: { op: 'all' }
            });
        }
    };

    tvheadend.idnode_simple(panel, {
        url: 'api/satips/config',
        title: _('SAT>IP Server'),
        iconCls: 'satipsrvconf',
        tabIndex: index,
        comet: 'satip_server',
        tbar: [discoverButton],
        help: function() {
            new tvheadend.help(_('SAT>IP Server Configuration'), 'config_satips.html');
        }
    });
};
