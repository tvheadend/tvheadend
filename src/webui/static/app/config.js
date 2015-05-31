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
        'server_name', 'muxconfpath', 'language',
        'tvhtime_update_enabled', 'tvhtime_ntp_enabled',
        'tvhtime_tolerance',
        'prefer_picon', 'chiconpath', 'piconpath',
        'satip_rtsp', 'satip_weight', 'satip_descramble', 'satip_muxcnf',
        'satip_dvbs', 'satip_dvbs2', 'satip_dvbt', 'satip_dvbt2',
        'satip_dvbc', 'satip_dvbc2', 'satip_atsc', 'satip_dvbcb'
    ]);

    /* ****************************************************************
    * Form Fields
    * ***************************************************************/

    /*
    * DVB path
    */

    var serverName = new Ext.form.TextField({
        fieldLabel: 'Tvheadend server name',
        name: 'server_name',
        allowBlank: true,
        width: 400
    });

    var serverWrap = new Ext.form.FieldSet({
        title: 'Server',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items : [ serverName ]
    });

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

    var tvhtimeUpdateEnabled = new Ext.ux.form.XCheckbox({
        name: 'tvhtime_update_enabled',
        fieldLabel: 'Update time'
    });

    var tvhtimeNtpEnabled = new Ext.ux.form.XCheckbox({
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

    /*
    * SAT>IP server
    */

    var satipPanel = null;
    if (tvheadend.capabilities.indexOf('satip_server') !== -1) {
        var rtsp = new Ext.form.NumberField({
             name: 'satip_rtsp',
             fieldLabel: 'RTSP Port (554 or 9983), 0 = disable'
        });
        var weight = new Ext.form.NumberField({
             name: 'satip_weight',
             fieldLabel: 'Subscription Weight'
        });
        var descramble = new Ext.form.NumberField({
             name: 'satip_descramble',
             fieldLabel: 'Descramble Services (Limit Per Mux)'
        });
        var muxcnf = new Ext.form.NumberField({
             name: 'satip_muxcnf',
             fieldLabel: 'Muxes Handling (0 = auto, 1 = keep, 2 = reject)'
        });
        var dvbs = new Ext.form.NumberField({
             name: 'satip_dvbs',
             fieldLabel: 'Exported DVB-S Tuners'
        });
        var dvbs2 = new Ext.form.NumberField({
             name: 'satip_dvbs2',
             fieldLabel: 'Exported DVB-S2 Tuners'
        });
        var dvbt = new Ext.form.NumberField({
             name: 'satip_dvbt',
             fieldLabel: 'Exported DVB-T Tuners'
        });
        var dvbt2 = new Ext.form.NumberField({
             name: 'satip_dvbt2',
             fieldLabel: 'Exported DVB-T2 Tuners'
        });
        var dvbc = new Ext.form.NumberField({
             name: 'satip_dvbc',
             fieldLabel: 'Exported DVB-C Tuners'
        });
        var dvbc2 = new Ext.form.NumberField({
             name: 'satip_dvbc2',
             fieldLabel: 'Exported DVB-C2 Tuners'
        });
        var atsc = new Ext.form.NumberField({
             name: 'satip_atsc',
             fieldLabel: 'Exported ATSC Tuners'
        });
        var dvbcb = new Ext.form.NumberField({
             name: 'satip_dvbcb',
             fieldLabel: 'Exported DVB-Cable/AnnexB Tuners'
        });

        satipPanel = new Ext.form.FieldSet({
            title: 'SAT>IP Server',
            width: 700,
            autoHeight: true,
            collapsible: true,
            collapsed: true,
            animCollapse: true,
            items: [rtsp, weight, descramble, muxcnf,
                    dvbs, dvbs2, dvbt, dvbt2, dvbc, dvbc2, atsc, dvbcb]
        });
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

    if (tvheadend.capabilities.indexOf('satip_client') !== -1) {
        var satipButton = new Ext.Button({
            text: "Discover SAT>IP servers",
            tooltip: 'Look for new SAT>IP servers',
            iconCls: 'find',
            handler: satipDiscover
        });
    } else {
        var satipButton = null;
    }


    var helpButton = new Ext.Button({
        text: 'Help',
        iconCls: 'help',
        handler: function() {
            new tvheadend.help('General Configuration', 'config_misc.html');
        }
    });

    var _items = [serverWrap, languageWrap, dvbscanWrap, tvhtimePanel, piconPanel];

    if (satipPanel)
      _items.push(satipPanel);

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

    var _items = [confpanel];

    if (imagecache_form)
        _items.push(imagecache_form);

    var _tbar = [saveButton, '-', imagecacheButton];
    if (satipButton) {
       _tbar.push('-');
       _tbar.push(satipButton);
    }
    _tbar.push('->');
    _tbar.push(helpButton);

    var mpanel = new Ext.Panel({
        title: 'General',
        iconCls: 'general',
        border: false,
        autoScroll: true,
        bodyStyle: 'padding:15px',
        layout: 'form',
        items: _items,
        tbar: _tbar
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

    function satipDiscover() {
        Ext.Ajax.request({
            url: 'api/hardware/satip/discover',
            params: { op: 'all' }
        });
    }
};
