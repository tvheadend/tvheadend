/*
 * Stream Profiles, Elementary Stream Filters
 */

tvheadend.caclient_builders = new Ext.data.JsonStore({
    url: 'api/profile/builders',
    root: 'entries',
    fields: ['class', 'caption', 'props'],
    id: 'class',
    autoLoad: true
});                    

tvheadend.esfilter_tab = function(panel)
{
    tvheadend.idnode_form_grid(panel, {
        url: 'api/profile',
        clazz: 'profile',
        comet: 'profile',
        titleS: 'Stream Profile',
        titleP: 'Stream Profiles',
        titleC: 'Stream Profile Name',
        iconCls: 'stream_profile',
        add: {
            url: 'api/profile',
            titleS: 'Stream Profile',
            select: {
                label: 'Type',
                store: tvheadend.caclient_builders,
                displayField: 'caption',
                valueField: 'class',
                propField: 'props'
            },
            create: { },
        },
        del: true,
        help: function() {
            new tvheadend.help('Stream Profile', 'config_profile.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/video',
        titleS: 'Video Stream Filter',
        titleP: 'Video Stream Filters',
        tabIndex: 1,
        add: {
            url: 'api/esfilter/video',
            create: {}
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
        tabIndex: 2,
        add: {
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
        tabIndex: 3,
        add: {
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
        tabIndex: 4,
        add: {
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
        tabIndex: 5,
        add: {
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
        tabIndex: 6,
        add: {
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
