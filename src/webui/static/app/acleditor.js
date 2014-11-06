/*
 * Access Control
 */

tvheadend.acleditor = function(panel, index)
{
    var list = 'enabled,username,password,prefix,' +
               'streaming,adv_streaming,profile,' +
               'dvr,dvr_config,webui,admin,conn_limit,' +
               'channel_min,channel_max,channel_tag,comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/access/entry',
        titleS: 'Access Entry',
        titleP: 'Access Entries',
        iconCls: 'group',
        columns: {
            enabled:       { width: 120 },
            username:      { width: 250 },
            password:      { width: 250 },
            prefix:        { width: 350 },
            streaming:     { width: 110 },
            adv_streaming: { width: 200 },
            dvr:           { width: 150 },
            webui:         { width: 140 },
            admin:         { width: 100 },
            conn_limit:    { width: 160 },
            channel_min:   { width: 160 },
            channel_max:   { width: 160 }
        },
        tabIndex: index,
        edit: {
            params: {
                list: list
            }
        },
        add: {
            url: 'api/access/entry',
            params: {
                list: list
            },
            create: { }
        },
        del: true,
        move: true,
        list: list,
        help: function() {
            new tvheadend.help('Access Control Entries', 'config_access.html');
        }
    });
};
