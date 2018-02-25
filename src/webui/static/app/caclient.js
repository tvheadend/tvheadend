/*
 * Conditional Access Client (cwc,capmt)
 */

tvheadend.caclient = function(panel, index) {

    if (!tvheadend.caclient_builders) {
        tvheadend.caclient_builders = new Ext.data.JsonStore({
            url: 'api/caclient/builders',
            root: 'entries',
            fields: ['class', 'caption', 'order', 'groups', 'props'],
            id: 'class',
            autoLoad: true
        });
    }

    var actions = new Ext.ux.grid.RowActions({
        id: 'status',
        header: '',
        width: 45,
        actions: [ { iconIndex: 'status' } ],
        destroy: function() { }
    });

    var list = 'enabled,name,username,password,hostname,mode,camdfilename,' +
               'port,cwmode,deskey,emm,emmex,caid,providerid,tsid,sid,' +
               'key_even,key_odd,keepalive_interval,comment,extended,' +
               'nodeid,version,limit,caid_select,caid_list,multi';

    tvheadend.idnode_form_grid(panel, {
        clazz: 'caclient',
        comet: 'caclient',
        titleS: _('CA'),
        titleP: _('CAs'),
        titleC: _('Client Name'),
        iconCls: 'key',
        key: 'uuid',
        val: 'title',
        fields: ['uuid', 'title', 'status'],
        tabIndex: index,
        uilevel: tvheadend.capabilities.indexOf('caclient_advanced') === -1
                 ? 'expert' : 'advanced',
        list: { url: 'api/caclient/list', params: { } },
        edit: { params: { list: list } },
        add: {
            url: 'api/caclient',
            titleS: _('Conditional Access Client'),
            select: {
                label: _('Type'),
                store: tvheadend.caclient_builders,
                fullRecord: true,
                displayField: 'caption',
                valueField: 'class',
                list: list
            },
            create: { }
        },
        del: true,
        move: true,
        hidepwd: true,
        lcol: [actions],
        plugins: [actions]
    });

    return panel;

}
