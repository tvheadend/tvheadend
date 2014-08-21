/*
 * Channel Tag
 */

tvheadend.cteditor = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/channeltag',
        comet: 'channeltag',
        titleS: 'Channel Tag',
        titleP: 'Channel Tags',
        tabIndex: index,
        add: {
            url: 'api/channeltag',
            create: { }
        },
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('Channel Tags', 'config_tags.html');
        },
    });

    return panel;
};
