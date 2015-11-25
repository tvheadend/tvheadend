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
        titleS: _('Stream profile'),
        titleP: _('Stream profiles'),
        titleC: _('Stream profile name'),
        iconCls: 'film',
        edit: { params: { list: list } },
        add: {
            url: 'api/profile',
            titleS: _('Stream profile'),
            select: {
                label: _('Type'),
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
            new tvheadend.help(_('Stream profile'), 'config_streamprofile.html');
        }
    });

    var eslist = '-class,index';

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/video',
        titleS: _('Video stream filter'),
        titleP: _('Video stream filters'),
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
            new tvheadend.help(_('Elementary stream filter'), 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/audio',
        titleS: _('Audio stream filter'),
        titleP: _('Audio stream filters'),
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
            new tvheadend.help(_('Elementary stream filter'), 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/teletext',
        titleS: _('Teletext stream filter'),
        titleP: _('Teletext stream filters'),
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
        titleS: _('Subtitle stream filter'),
        titleP: _('Subtitle stream filters'),
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
            new tvheadend.help(_('Elementary stream filter'), 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/ca',
        titleS: _('CA stream filter'),
        titleP: _('CA stream filters'),
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
            new tvheadend.help(_('Elementary stream filter'), 'config_esfilter.html');
        }
    });

    tvheadend.idnode_grid(panel, {
        url: 'api/esfilter/other',
        titleS: _('Other stream filter'),
        titleP: _('Other stream filters'),
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
            new tvheadend.help(_('Elementary stream filter'), 'config_esfilter.html');
        }
    });
};
