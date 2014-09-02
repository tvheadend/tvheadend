tvheadend.tvadapters = function(panel, index) {

    tvheadend.idnode_tree(panel, {
        url: 'api/hardware/tree',
        title: 'TV adapters',
        tabIndex: index,
        help: function() {
            new tvheadend.help('TV adapters', 'config_tvadapters.html');
        }
    });

    return panel;
};
