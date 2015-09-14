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

/*
tvheadend.comet.on('config', function(m) {
    if (m.reload != null) {
        tvheadend.languages.reload();
        tvheadend.config_languages.reload();
    }
});
*/

/*
 * Base configuration
 */

tvheadend.baseconf = function(panel, index) {

/*
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
*/

    tvheadend.idnode_simple(panel, {
        url: 'api/config',
        title: _('Base'),
        iconCls: 'baseconf',
        tabIndex: index,
        comet: 'config',
        labelWidth: 250,
        help: function() {
            new tvheadend.help(_('General Configuration'), 'config_general.html');
        }
    });

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
        width: 550,
        labelWidth: 200,
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
        width: 600,
        labelWidth: 250,
        tbar: [discoverButton],
        help: function() {
            new tvheadend.help(_('SAT>IP Server Configuration'), 'config_satips.html');
        }
    });
};
