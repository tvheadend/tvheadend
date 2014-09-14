/*
 * Access Control
 */

tvheadend.acleditor = function(panel, index)
{
    var list = 'enabled,username,password,prefix,streaming,adv_streaming,' +
               'dvr,dvr_config,webui,admin,channel_min,channel_max,channel_tag,' +
               'comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/access/entry',
        titleS: 'Access Entry',
        titleP: 'Access Entries',
        iconCls: 'group',
        columns: {
            username:      { width: 250 },
            password:      { width: 250 },
            prefix:        { width: 350 },
            streaming:     { width: 100 },
            adv_streaming: { width: 100 },
            dvr:           { width: 100 },
            webui:         { width: 100 },
            admin:         { width: 100 },
            channel_min:   { width: 100 },
            channel_max:   { width: 100 },
        },
        tabIndex: index,
        edit: {
            params: {
                list: list,
            },
        },
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
};
