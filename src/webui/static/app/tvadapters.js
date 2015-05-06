tvheadend.tvadapters = function(panel, index) {

    tvheadend.idnode_tree(panel, {
        url: 'api/hardware/tree',
        title: 'TV adapters',
        iconCls: 'tvCards',
        tabIndex: index,
        comet: 'hardware',
        help: function() {
            new tvheadend.help('TV adapters', 'config_tvadapters.html');
        }
    });

    return panel;
};
