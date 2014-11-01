/*
 * Channel Tag
 */

tvheadend.cteditor = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/channeltag',
        titleS: 'Channel Tag',
        titleP: 'Channel Tags',
        iconCls: 'channelTags',
        tabIndex: index,
        add: {
            url: 'api/channeltag',
            create: { }
        },
        del: true,
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('Channel Tags', 'config_tags.html');
        }
    });

    return panel;
};

/*
 * Bouquet
 */
tvheadend.bouquet = function(panel, index)
{
    var list = 'enabled,name,maptoch,mapnolcn,lcn_off,mapnoname,chtag,source,services_count,comment';

    tvheadend.idnode_grid(panel, {
        url: 'api/bouquet',
        titleS: 'Bouquet',
        titleP: 'Bouquets',
        iconCls: 'bouquets',
        tabIndex: index,
        columns: {
            enabled:        { width: 50 },
            name:           { width: 200 },
            maptoch:        { width: 100 },
            mapnolcn:       { width: 100 },
            lcn_off:        { width: 100 },
            mapnoname:      { width: 100 },
            chtag:          { width: 100 },
            source:         { width: 200 },
            services_count: { width: 100 },
            comment:        { width: 200 },
        },
        list: list,
        del: true,
        edit: { params: { list: list } },
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('Bouquets', 'config_bouquet.html');
        },
    });

    return panel;
};
