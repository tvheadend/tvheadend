/*
 * Access Control
 */

tvheadend.acleditor = function(panel)
{
    panel = new Ext.TabPanel({
        activeTab: 0,
        autoScroll: true,
        title: 'Access Control',
        iconCls: 'group',
        items: []
    });

    var list = 'enabled,username,password,prefix,streaming,adv_streaming,' +
               'dvr,dvr_config,webui,admin,channel_min,channel_max,channel_tag,' +
               'comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/access/entry',
        comet: 'acl_entries',
        titleS: 'Access Entry',
        titleP: 'Access Entries',
        tabIndex: 0,
        add: {
            url: 'api/access/entry',
            params: {
                list: list,
            },
            create: { }
        },
        del: true,
        move: true,
        list: list,
        help: function() {
            new tvheadend.help('Access Control Entries', 'config_access.html');
        },
    });

    return panel;
};
