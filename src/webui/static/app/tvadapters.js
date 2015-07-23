tvheadend.tvadapters = function(panel, index) {

    tvheadend.idnode_tree(panel, {
        url: 'api/hardware/tree',
        title: _('TV adapters'),
        iconCls: 'tvCards',
        tabIndex: index,
        comet: 'hardware',
        help: function() {
            new tvheadend.help(_('TV adapters'), 'config_tvadapters.html');
        }
    });

    return panel;
};
