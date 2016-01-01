/*
 * Base configuration
 */

tvheadend.baseconf = function(panel, index) {

    var wizardButton = {
        name: 'wizard',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Start initial configuration wizard'),
                iconCls: 'wizard',
                text: _('Start wizard')
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
                url: 'api/wizard/start',
                success: function() {
                    window.location.reload();
                }
            });
        }
    };

    tvheadend.idnode_simple(panel, {
        id: 'base_config',
        url: 'api/config',
        title: _('Base'),
        iconCls: 'baseconf',
        tabIndex: index,
        comet: 'config',
        labelWidth: 250,
        tbar: [wizardButton],
        postsave: function(data) {
            var l = data['uilevel'];
            if (l >= 0) {
                var tr = {0:'basic',1:'advanced',2:'expert'};
                l = (l in tr) ? tr[l] : 'basic';
                if (l !== tvheadend.uilevel) {
                    window.location.reload();
                    return;
                }
            }
            var n = data['uilevel_nochange'] ? true : false;
            if (n !== tvheadend.uilevel_nochange)
                window.location.reload();
        },
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
                iconCls: 'fetch_images',
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
