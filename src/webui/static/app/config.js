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

    var imagecache_reader = new Ext.data.JsonReader({root: 'entries'},
        [ 'enabled', 'ok_period', 'fail_period', 'ignore_sslcert' ]);

    var imagecacheEnabled = new Ext.ux.form.XCheckbox({
        name: 'enabled',
        fieldLabel: _('Enabled')
    });

    var imagecacheOkPeriod = new Ext.form.NumberField({
        name: 'ok_period',
        fieldLabel: _('Re-fetch period (hours)')
    });

    var imagecacheFailPeriod = new Ext.form.NumberField({
        name: 'fail_period',
        fieldLabel: _('Re-try period (hours)')
    });

    var imagecacheIgnoreSSLCert = new Ext.ux.form.XCheckbox({
        name: 'ignore_sslcert',
        fieldLabel: _('Ignore invalid SSL certificate')
    });

    var imagecachePanel = new Ext.form.FieldSet({
        title: _('Image Caching'),
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
        labelWidth: 300,
        waitMsgTarget: true,
        reader: imagecache_reader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: [imagecachePanel]
    });

    var saveButton = new Ext.Button({
        text: _("Save configuration"),
        tooltip: _('Save changes made to configuration below'),
        iconCls: 'save',
        handler: saveChanges
    });

    var imagecacheButton = new Ext.Button({
        text: _("Clean image (icon) cache"),
        tooltip: _('Clean image cache on storage'),
        iconCls: 'clean',
        handler: cleanImagecache
    });

    var _tbar = [saveButton, '-', imagecacheButton];

    var mpanel = new Ext.Panel({
        title: _('Image cache'),
        iconCls: 'imgcacheconf',
        border: false,
        autoScroll: true,
        bodyStyle: 'padding:15px',
        layout: 'form',
        items: [imagecache_form],
        tbar: _tbar
    });

    tvheadend.paneladd(panel, mpanel, index);

    mpanel.on('render', function() {
        imagecache_form.getForm().load({
            url: 'api/imagecache/config/load',
            success: function(form, action) {
                imagecache_form.enable();
            },
            failure: function(form, action) {
                alert(_("FAILED"));
            }
        });
    });

    function saveChanges(params) {
        if (imagecache_form)
            imagecache_form.getForm().submit({
                url: 'api/imagecache/config/save',
                params: params || {},
                waitMsg: _('Saving data...'),
                failure: function(form, action) {
                    Ext.Msg.alert(_('Imagecache save failed'), action.result.errormsg);
                }
            });
    }

    function cleanImagecache() {
        saveChanges({'clean': 1});
    }
};

/*
 * SAT>IP server configuration
 */

tvheadend.satipsrvconf = function(panel, index) {

    if (tvheadend.capabilities.indexOf('satip_server') === -1)
        return;

    var satipsrv_reader = new Ext.data.JsonReader({root: 'entries'},
        [ 'satip_rtsp', 'satip_weight', 'satip_descramble', 'satip_muxcnf',
          'satip_dvbs', 'satip_dvbs2', 'satip_dvbt', 'satip_dvbt2',
          'satip_dvbc', 'satip_dvbc2', 'satip_atsc', 'satip_dvbcb' ]);

    var rtsp = new Ext.form.NumberField({
         name: 'satip_rtsp',
         fieldLabel: _('RTSP Port (554 or 9983), 0 = disable')
    });
    var weight = new Ext.form.NumberField({
         name: 'satip_weight',
         fieldLabel: _('Subscription Weight')
    });
    var descramble = new Ext.form.NumberField({
         name: 'satip_descramble',
         fieldLabel: _('Descramble Services (Limit Per Mux)')
    });
    var muxcnf = new Ext.form.NumberField({
         name: 'satip_muxcnf',
         fieldLabel: _('Mux Handling (0 = auto, 1 = keep, 2 = reject)')
    });
    var dvbs = new Ext.form.NumberField({
         name: 'satip_dvbs',
         fieldLabel: _('Exported DVB-S Tuners')
    });
    var dvbs2 = new Ext.form.NumberField({
         name: 'satip_dvbs2',
         fieldLabel: _('Exported DVB-S2 Tuners')
    });
    var dvbt = new Ext.form.NumberField({
         name: 'satip_dvbt',
         fieldLabel: _('Exported DVB-T Tuners')
    });
    var dvbt2 = new Ext.form.NumberField({
         name: 'satip_dvbt2',
         fieldLabel: _('Exported DVB-T2 Tuners')
    });
    var dvbc = new Ext.form.NumberField({
         name: 'satip_dvbc',
         fieldLabel: _('Exported DVB-C Tuners')
    });
    var dvbc2 = new Ext.form.NumberField({
         name: 'satip_dvbc2',
         fieldLabel: _('Exported DVB-C2 Tuners')
    });
    var atsc = new Ext.form.NumberField({
         name: 'satip_atsc',
         fieldLabel: _('Exported ATSC Tuners')
    });
    var dvbcb = new Ext.form.NumberField({
         name: 'satip_dvbcb',
         fieldLabel: _('Exported DVB-Cable/AnnexB Tuners')
    });

    satipPanel = new Ext.form.FieldSet({
        title: _('SAT>IP Server'),
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [rtsp, weight, descramble, muxcnf,
                dvbs, dvbs2, dvbt, dvbt2, dvbc, dvbc2, atsc, dvbcb]
    });

    var satipsrv_form = new Ext.form.FormPanel({
        border: false,
        labelAlign: 'left',
        labelWidth: 300,
        waitMsgTarget: true,
        reader: satipsrv_reader,
        layout: 'form',
        defaultType: 'textfield',
        autoHeight: true,
        items: [satipPanel]
    });

    var saveButton = new Ext.Button({
        text: _("Save configuration"),
        tooltip: _('Save changes made to configuration below'),
        iconCls: 'save',
        handler: saveChanges
    });

    var satipButton = new Ext.Button({
        text: _("Discover SAT>IP servers"),
        tooltip: _('Look for new SAT>IP servers'),
        iconCls: 'find',
        handler: satipDiscover
    });

    var _tbar = [saveButton, '-', satipButton];

    var mpanel = new Ext.Panel({
        title: _('SAT>IP Server'),
        iconCls: 'satipsrvconf',
        border: false,
        autoScroll: true,
        bodyStyle: 'padding:15px',
        layout: 'form',
        items: [satipsrv_form],
        tbar: _tbar
    });

    tvheadend.paneladd(panel, mpanel, index);

    mpanel.on('render', function() {
        satipsrv_form.getForm().load({
            url: 'api/satips/config/load',
            success: function(form, action) {
                satipsrv_form.enable();
            },
            failure: function(form, action) {
                alert(_("FAILED"));
            }
        });
    });

    function saveChanges(params) {
        if (satipsrv_form)
            satipsrv_form.getForm().submit({
                url: 'api/satips/config/save',
                params: params || {},
                waitMsg: _('Saving data...'),
                failure: function(form, action) {
                    Ext.Msg.alert(_('SAT>IP Server configuration save failed'), action.result.errormsg);
                }
            });
    }

    function satipDiscover() {
        Ext.Ajax.request({
            url: 'api/hardware/satip/discover',
            params: { op: 'all' }
        });
    }
};
