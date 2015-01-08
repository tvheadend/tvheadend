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

tvheadend.miscconf = function(panel, index) {

    /*
    * Basic Config
    */

    var confreader = new Ext.data.JsonReader({
        root: 'config'
    },
    [
        'muxconfpath', 'language',
        'tvhtime_update_enabled', 'tvhtime_ntp_enabled',
        'tvhtime_tolerance',
        'prefer_picon',
        'chiconpath',
        'piconpath'
    ]);

    /* ****************************************************************
    * Form Fields
    * ***************************************************************/

    /*
    * DVB path
    */

    var dvbscanPath = new Ext.form.TextField({
        fieldLabel: 'DVB scan files path',
        name: 'muxconfpath',
        allowBlank: true,
        width: 400
    });

    var dvbscanWrap = new Ext.form.FieldSet({
        title: 'DVB Scan Files',
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
        fieldLabel: 'Default Language(s)',
        dataFields: ['identifier', 'name'],
        msWidth: 190,
        msHeight: 150,
        valueField: 'identifier',
        displayField: 'name',
        imagePath: 'static/multiselect/resources',
        toLegend: 'Selected',
        fromLegend: 'Available'
    });

    var languageWrap = new Ext.form.FieldSet({
        title: 'Language Settings',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [ language ]
    });

    /*
    * Time/Date
    */

    var tvhtimeUpdateEnabled = new Ext.form.Checkbox({
        name: 'tvhtime_update_enabled',
        fieldLabel: 'Update time'
    });

    var tvhtimeNtpEnabled = new Ext.form.Checkbox({
        name: 'tvhtime_ntp_enabled',
        fieldLabel: 'Enable NTP driver'
    });

    var tvhtimeTolerance = new Ext.form.NumberField({
        name: 'tvhtime_tolerance',
        fieldLabel: 'Update tolerance (ms)'
    });

    var tvhtimePanel = new Ext.form.FieldSet({
        title: 'Time Update',
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
        fieldLabel: 'Prefer picons over channel name'
    });

    var chiconPath = new Ext.form.TextField({
        name: 'chiconpath',
        fieldLabel: 'Channel icon path (see Help)',
        width: 400
    });

    var piconPath = new Ext.form.TextField({
        name: 'piconpath',
        fieldLabel: 'Picon path (see Help)',
        width: 400
    });

    var piconPanel = new Ext.form.FieldSet({
        title: 'Picon',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [preferPicon, chiconPath, piconPath]
    });

    /*
    * Image cache
    */

    if (tvheadend.capabilities.indexOf('imagecache') !== -1) {
        var imagecache_reader = new Ext.data.JsonReader({
            root: 'entries'
        },
        [
            'enabled', 'ok_period', 'fail_period', 'ignore_sslcert'
        ]);

        var imagecacheEnabled = new Ext.ux.form.XCheckbox({
            name: 'enabled',
            fieldLabel: 'Enabled'
        });

        var imagecacheOkPeriod = new Ext.form.NumberField({
            name: 'ok_period',
            fieldLabel: 'Re-fetch period (hours)'
        });

        var imagecacheFailPeriod = new Ext.form.NumberField({
            name: 'fail_period',
            fieldLabel: 'Re-try period (hours)'
        });

        var imagecacheIgnoreSSLCert = new Ext.ux.form.XCheckbox({
            name: 'ignore_sslcert',
            fieldLabel: 'Ignore invalid SSL certificate'
        });

        var imagecachePanel = new Ext.form.FieldSet({
            title: 'Image Caching',
            width: 700,
            autoHeight: true,
            collapsible: true,
            animCollapse: true,
            items: [imagecacheEnabled, imagecacheOkPeriod, imagecacheFailPeriod,
                imagecacheIgnoreSSLCert]
        });

        var imagecache_form = new Ext.form.FormPanel({
            border: false,
            labelAlign: 'left',
            labelWidth: 200,
            waitMsgTarget: true,
            reader: imagecache_reader,
            layout: 'form',
            defaultType: 'textfield',
            autoHeight: true,
            items: [imagecachePanel]
        });
    } else {
        var imagecache_form = null;
    }

    /* ****************************************************************
    * Form
    * ***************************************************************/

    var saveButton = new Ext.Button({
        text: "Save configuration",
        tooltip: 'Save changes made to configuration below',
        iconCls: 'save',
        handler: saveChanges
    });

    var imagecacheButton = new Ext.Button({
        text: "Clean image (icon) cache",
        tooltip: 'Clean image cache on storage',
        iconCls: 'clean',
        handler: cleanImagecache
    });

    var helpButton = new Ext.Button({
        text: 'Help',
		iconCls: 'help',
        handler: function() {
            new tvheadend.help('General Configuration', 'config_misc.html');
        }
    });

    var confpanel = new Ext.form.FormPanel({
        labelAlign: 'left',
        labelWidth: 200,
        border: false,
        waitMsgTarget: true,
        reader: confreader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: [languageWrap, dvbscanWrap, tvhtimePanel, piconPanel]
    });

    var _items = [confpanel];

    if (imagecache_form)
        _items.push(imagecache_form);

    var mpanel = new Ext.Panel({
        title: 'General',
        iconCls: 'general',
        border: false,
        autoScroll: true,
        bodyStyle: 'padding:15px',
        layout: 'form',
        items: _items,
        tbar: [saveButton, '-', imagecacheButton, '->', helpButton]
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
        if (imagecache_form)
            imagecache_form.getForm().load({
                url: 'api/imagecache/config/load',
                success: function(form, action) {
                    imagecache_form.enable();
                },
                failure: function(form, action) {
                    alert("FAILED");
                }
            });
    });

    function saveChangesImagecache(params) {
        if (imagecache_form)
            imagecache_form.getForm().submit({
                url: 'api/imagecache/config/save',
                params: params || {},
                waitMsg: 'Saving data...',
                failure: function(form, action) {
                    Ext.Msg.alert('Imagecache save failed', action.result.errormsg);
                }
            });
    }

    function saveChanges() {
        confpanel.getForm().submit({
            url: 'config',
            params: {
                op: 'saveSettings'
            },
            waitMsg: 'Saving Data...',
            failure: function(form, action) {
                Ext.Msg.alert('Save failed', action.result.errormsg);
            }
        });
        saveChangesImagecache();
    }

    function cleanImagecache() {
        saveChangesImagecache({'clean': 1});
    }
};
