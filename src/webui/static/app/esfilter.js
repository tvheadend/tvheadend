/*
 * Stream Profiles, Elementary Stream Filters
 */

tvheadend.esfilter_tab = function(panel)
{
    if (!tvheadend.profile_builders) {
        tvheadend.profile_builders = new Ext.data.JsonStore({
            url: 'api/profile/builders',
            root: 'entries',
            fields: ['class', 'caption', 'props'],
            id: 'class',
            autoLoad: true
        });
    }

    var list = '-class';

    tvheadend.idnode_form_grid(panel, {
        url: 'api/profile',
        clazz: 'profile',
        comet: 'profile',
        titleS: 'Stream Profile',
        titleP: 'Stream Profiles',
        titleC: 'Stream Profile Name',
        iconCls: 'film',
        edit: { params: { list: list } },
        add: {
            url: 'api/profile',
            titleS: 'Stream Profile',
            select: {
                label: 'Type',
                store: tvheadend.profile_builders,
                displayField: 'caption',
                valueField: 'class',
                propField: 'props',
                list: list
            },
            create: { }
        },
        del: true,
        help: function() {
            new tvheadend.help('Stream Profile', 'config_streamprofile.html');
        }
    });

    var eslist = '-class,index';

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/video',
        titleS: 'Video Stream Filter',
        titleP: 'Video Stream Filters',
        iconCls: 'film_edit',
        tabIndex: 1,
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/video',
            create: { }
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/audio',
        titleS: 'Audio Stream Filter',
        titleP: 'Audio Stream Filters',
        iconCls: 'control_volume',
        tabIndex: 2,
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/audio',
            create: {}
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/teletext',
        titleS: 'Teletext Stream Filter',
        titleP: 'Teletext Stream Filters',
		iconCls: 'teletext',
        tabIndex: 3,
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/teletext',
            create: {}
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/subtit',
        titleS: 'Subtitle Stream Filter',
        titleP: 'Subtitle Stream Filters',
		iconCls: 'subtitle',
        tabIndex: 4,
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/subtit',
            create: {}
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/ca',
        titleS: 'CA Stream Filter',
        titleP: 'CA Stream Filters',
        iconCls: 'film_key',
        tabIndex: 5,
        add: {
            params: { list: eslist },
            url: 'api/esfilter/ca',
            create: {}
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/other',
        titleS: 'Other Stream Filter',
        titleP: 'Other Stream Filters',
		iconCls: 'otherFilters',
        tabIndex: 6,
        edit: { params: { list: eslist } },
        add: {
            params: { list: eslist },
            url: 'api/esfilter/other',
            create: {}
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Elementary Stream Filter', 'config_esfilter.html');
        }
    });
};
