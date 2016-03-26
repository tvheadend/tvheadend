/*
 * Access Control
 */

tvheadend.acleditor = function(panel, index)
{
    var list = 'enabled,username,password,prefix,' +
               'lang,webui,uilevel,uilevel_nochange,admin,' +
               'streaming,adv_streaming,htsp_streaming,' +
               'profile,conn_limit_type,conn_limit,' +
               'dvr,htsp_dvr,all_dvr,all_rw_dvr,' +
	       'dvr_config,channel_min,channel_max,' +
	       'channel_tag_exclude,channel_tag,comment';

    var list2 = 'enabled,username,password,prefix,' +
                'lang,webui,themeui,langui,uilevel,uilevel_nochange,admin,' +
                'streaming,adv_streaming,htsp_streaming,' +
                'profile,conn_limit_type,conn_limit,' +
                'dvr,htsp_dvr,all_dvr,all_rw_dvr,' +
                'failed_dvr,htsp_anonymize,dvr_config,' +
                'channel_min,channel_max,channel_tag_exclude,' +
                'channel_tag,comment';

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
            streaming:      { width: 110 },
            adv_streaming:  { width: 200 },
            htsp_streaming: { width: 200 },
            dvr:            { width: 150 },
            htsp_dvr:       { width: 150 },
            all_dvr:        { width: 150 },
            all_rw_dvr:     { width: 150 },
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
        list: list,
        help: function() {
            new tvheadend.help(_('Access Control Entries'), 'config_access.html');
        }
    });
};

/*
 * Password Control
 */

tvheadend.passwdeditor = function(panel, index)
{
    var list = 'enabled,username,password,comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/passwd/entry',
        titleS: _('Password'),
        titleP: _('Passwords'),
        iconCls: 'pass',
        columns: {
            enabled:  { width: 120 },
            username: { width: 250 },
            password: { width: 250 }
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
        list: list,
        help: function() {
            new tvheadend.help(_('Password Control Entries'), 'config_passwords.html');
        }
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
        list: list,
        help: function() {
            new tvheadend.help(_('IP Blocking Entries'), 'config_ipblock.html');
        }
    });
};
