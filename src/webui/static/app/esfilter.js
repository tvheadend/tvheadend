/*
 * Stream Profiles, Elementary Stream Filters
 */

tvheadend.esfilter_tab = function(panel)
{
    if (!tvheadend.profile_builders) {
        tvheadend.profile_builders = new Ext.data.JsonStore({
            url: 'api/profile/builders',
            root: 'entries',
            fields: ['class', 'caption', 'order', 'groups', 'params'],
            id: 'class',
            autoLoad: true
        });
    }

    var list = '-class';

    tvheadend.idnode_form_grid(panel, {
        url: 'api/profile',
        clazz: 'profile',
        comet: 'profile',
        titleS: _('Stream Profile'),
        titleP: _('Stream Profiles'),
        titleC: _('Stream Profile Name'),
        iconCls: 'film',
        edit: { params: { list: list } },
        add: {
            url: 'api/profile',
            titleS: _('Stream Profile'),
            select: {
                label: _('Type'),
                store: tvheadend.profile_builders,
                fullRecord: true,
                displayField: 'caption',
                valueField: 'class',
                list: list
            },
            create: { }
        },
        del: true
    });

    var eslist = '-class,index';

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/video',
        titleS: _('Video Stream Filter'),
        titleP: _('Video Stream Filters'),
        iconCls: 'film_edit',
        tabIndex: 1,
        uilevel: 'expert',
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/video',
            create: { }
        },
        del: true,
        move: true
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/audio',
        titleS: _('Audio Stream Filter'),
        titleP: _('Audio Stream Filters'),
        iconCls: 'control_volume',
        tabIndex: 2,
        uilevel: 'expert',
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/audio',
            create: {}
        },
        del: true,
        move: true
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/teletext',
        titleS: _('Teletext Stream Filter'),
        titleP: _('Teletext Stream Filters'),
        iconCls: 'teletext',
        tabIndex: 3,
        uilevel: 'expert',
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/teletext',
            create: {}
        },
        del: true,
        move: true
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/subtit',
        titleS: _('Subtitle Stream Filter'),
        titleP: _('Subtitle Stream Filters'),
        iconCls: 'subtitle',
        tabIndex: 4,
        uilevel: 'expert',
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/subtit',
            create: {}
        },
        del: true,
        move: true
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/ca',
        titleS: _('CA Stream Filter'),
        titleP: _('CA Stream Filters'),
        iconCls: 'film_key',
        tabIndex: 5,
        uilevel: 'expert',
        add: {
            params: { list: eslist },
            url: 'api/esfilter/ca',
            create: {}
        },
        del: true,
        move: true
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/other',
        titleS: _('Other Stream Filter'),
        titleP: _('Other Stream Filters'),
        iconCls: 'otherFilters',
        tabIndex: 6,
        uilevel: 'expert',
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/other',
            create: {}
        },
        del: true,
        move: true
    });
};
