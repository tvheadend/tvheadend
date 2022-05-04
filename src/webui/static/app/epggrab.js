tvheadend.epggrab_rerun_button = function() {
    return {
        name: 'trigger',
        builder: function() {
            return new Ext.Toolbar.Button({
                text: _("Re-run Internal EPG Grabbers"),
                tooltip: _('Re-run all internal EPG grabbers to import EPG data now'),
                iconCls: 'find',
            });
        },
        callback: function(conf) {
            tvheadend.Ajax({
               url: 'api/epggrab/internal/rerun',
               params: { rerun: 1 },
            });
        }
    };
}

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
        tbar: [triggerButton, tvheadend.epggrab_rerun_button()]
    });

}

tvheadend.epggrab_map = function(panel, index) {

    tvheadend.idnode_grid(panel, {
        url: 'api/epggrab/channel',
        all: 1,
        titleS: _('EPG Grabber Channel'),
        titleP: _('EPG Grabber Channels'),
        iconCls: 'baseconf',
        tabIndex: index,
        uilevel: 'expert',
        del: true,
        sort: {
          field: 'name',
          direction: 'ASC'
        }
    });

    return panel;

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
        uilevel: 'advanced',
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
        tbar: [tvheadend.epggrab_rerun_button()]
    });

};
