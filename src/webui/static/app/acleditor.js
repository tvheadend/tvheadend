/*
 * Access Control
 */

tvheadend.acleditor = function(panel, index)
{
    var list = 'enabled,username,password,prefix,change,' +
               'lang,webui,uilevel,uilevel_nochange,admin,' +
               'streaming,profile,conn_limit_type,conn_limit,' +
               'dvr,dvr_config,channel_min,channel_max,' +
	       'channel_tag_exclude,channel_tag,comment';

    var list2 = 'enabled,username,password,prefix,change,' +
                'lang,webui,themeui,langui,uilevel,uilevel_nochange,admin,' +
                'streaming,profile,conn_limit_type,conn_limit,' +
                'dvr,htsp_anonymize,dvr_config,' +
                'channel_min,channel_max,channel_tag_exclude,' +
                'channel_tag,xmltv_output_format,htsp_output_format,comment';

    tvheadend.idnode_grid(panel, {
        id: 'access_entry',
        url: 'api/access/entry',
        titleS: _('Access Entry'),
        titleP: _('Access Entries'),
        iconCls: 'acl',
        columns: {
            enabled:        { width: 120 },
            username:       { width: 250 },
            password:       { width: 250 },
            prefix:         { width: 350 },
            change:         { width: 350 },
            streaming:      { width: 350 },
            dvr:            { width: 350 },
            webui:          { width: 140 },
            admin:          { width: 100 },
            conn_limit_type:{ width: 160 },
            conn_limit:     { width: 160 },
            channel_min:    { width: 160 },
            channel_max:    { width: 160 }
        },
        tabIndex: index,
        edit: {
            params: {
                list: list2
            }
        },
        add: {
            url: 'api/access/entry',
            params: {
                list: list2
            },
            create: { }
        },
        del: true,
        move: true,
        list: list
    });
};

/*
 * Password Control
 */

tvheadend.passwdeditor = function(panel, index)
{
    var list = 'enabled,username,password,auth,authcode,comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/passwd/entry',
        titleS: _('Password'),
        titleP: _('Passwords'),
        iconCls: 'pass',
        columns: {
            enabled:  { width: 120 },
            username: { width: 250 },
            password: { width: 250 },
            auth:     { width: 250 },
            authcode: { width: 250 }
        },
        tabIndex: index,
        edit: {
            params: {
                list: list
            }
        },
        add: {
            url: 'api/passwd/entry',
            params: {
                list: list
            },
            create: { }
        },
        del: true,
        list: list
    });
};

/*
 * IP Blocking Control
 */

tvheadend.ipblockeditor = function(panel, index)
{
    var list = 'enabled,prefix,comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/ipblock/entry',
        titleS: _('IP Blocking Record'),
        titleP: _('IP Blocking Records'),
        iconCls: 'ip_block',
        columns: {
            enabled: { width: 120 },
            prefix:  { width: 350 },
            comment: { width: 250 }
        },
        tabIndex: index,
        uilevel: 'expert',
        edit: {
            params: {
                list: list
            }
        },
        add: {
            url: 'api/ipblock/entry',
            params: {
                list: list
            },
            create: { }
        },
        del: true,
        list: list
    });
};
