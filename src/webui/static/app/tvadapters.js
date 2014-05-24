tvheadend.tvadapters = function() {
    return tvheadend.idnode_tree({
        url: 'api/hardware/tree',
        title: 'TV adapters',
        comet: 'hardware',
        help: function() {
            new tvheadend.help('TV adapters', 'config_tvadapters.html');
        }
    });
};
