/*
 * Elementary Stream Filters
 */

tvheadend.esfilter_tab = function(panel)
{
    if (tvheadend.uilevel_match('expert', tvheadend.uilevel)) {
        var tab = new Ext.TabPanel({
            activeTab: 0,
            autoScroll: true,
            title: _('Stream Filters'),
            iconCls: 'otherFilters',
            items: []
        });
        tvheadend.esfilter_tabs(tab);
        panel.add(tab);
    }
};

tvheadend.esfilter_tabs = function(panel)
{
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
