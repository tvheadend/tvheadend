tvheadend.tvadapters = function(panel, index) {

    tvheadend.idnode_tree(panel, {
        id: 'tvadapters',
        url: 'api/hardware/tree',
        title: _('TV adapters'),
        iconCls: 'tvCards',
        tabIndex: index,
        comet: 'hardware'
    });

    return panel;
};
