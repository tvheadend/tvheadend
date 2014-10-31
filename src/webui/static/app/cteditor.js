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
    var list = 'enabled,name,maptoch,source,services_count,comment,lcn_off';

    tvheadend.idnode_grid(panel, {
        url: 'api/bouquet',
        titleS: 'Bouquet',
        titleP: 'Bouquets',
        iconCls: 'bouquets',
        tabIndex: index,
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
