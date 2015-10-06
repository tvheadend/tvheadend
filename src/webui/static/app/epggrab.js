tvheadend.epggrab_base = function(panel, index) {

    var triggerButton = {
        name: 'trigger',
        builder: function() {
            return new Ext.Toolbar.Button({
                text: _("Trigger OTA EPG Grabber"),
                tooltip: _('Tune to the over-the-air EPG muxes to grab new events now'),
                iconCls: 'find',
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
               url: 'api/epggrab/ota/trigger',
               params: { trigger: 1 },
            });
        }
    };

    tvheadend.idnode_simple(panel, {
        url: 'api/epggrab/config',
        title: _('EPG Grabber'),
        iconCls: 'baseconf',
        comet: 'epggrab',
        tabIndex: index,
        width: 550,
        labelWidth: 200,
        tbar: [triggerButton],
        help: function() {
            new tvheadend.help(_('EPG Grab Configuration'), 'config_epggrab.html');
        }
    });

}

tvheadend.epggrab_mod = function(panel, index) {

    var actions = new Ext.ux.grid.RowActions({
        id: 'status',
        header: '',
        width: 45,
        actions: [ { iconIndex: 'status' } ],
        destroy: function() { }
    });

    tvheadend.idnode_form_grid(panel, {
        tabIndex: index,
        clazz: 'epggrab_mod',
        comet: 'epggrab_mod',
        titleS: _('EPG Grabber Module'),
        titleP: _('EPG Grabber Modules'),
        titleC: _('EPG Grabber Name'),
        iconCls: 'baseconf',
        key: 'uuid',
        val: 'title',
        fields: ['uuid', 'title', 'status'],
        list: { url: 'api/epggrab/module/list', params: { } },
        lcol: [actions],
        plugins: [actions],
        help: function() {
            new tvheadend.help(_('EPG Grab Configuration'), 'config_epggrab.html');
        }
    });

};
