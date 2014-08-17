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

    tvheadend.idnode_grid(panel, {
        url: 'api/access/entry',
        comet: 'acl_entries',
        titleS: 'Access Entry',
        titleP: 'Access Entries',
        tabIndex: 0,
        add: {
            url: 'api/access/entry',
            create: { }
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Access Control Entries', 'config_access.html');
        },
    });

    return panel;
};
