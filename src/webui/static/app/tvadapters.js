tvheadend.tvadapters = function(panel, index) {

    tvheadend.idnode_tree(panel, {
        id: 'tvadapters',
        url: 'api/hardware/tree',
        title: _('TV adapters'),
        iconCls: 'tvCards',
        tabIndex: index,
        comet: 'hardware',
        node_added: function(n) {
            var p = n.attributes.params;
            if (!p) return;
            for (var i = 0; i < p.length; i++)
                if (p[i].id == "active" && p[i].value) {
                    n.ui.addClass('x-tree-node-on');
                    break;
                }
            if (i >= p.length)
                    n.ui.addClass('x-tree-node-off');
        }
    });

    return panel;
};
