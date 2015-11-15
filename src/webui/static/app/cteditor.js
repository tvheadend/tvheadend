/*
 * Channel Tag
 */

tvheadend.cteditor = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/channeltag',
        all: 1,
        titleS: _('Channel Tag'),
        titleP: _('Channel Tags'),
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
            new tvheadend.help(_('Channel Tags'), 'config_tags.html');
        }
    });

    return panel;
};

/*
 * Bouquet
 */
tvheadend.bouquet = function(panel, index)
{
    var list0 = 'name,maptoch,mapnolcn,lcn_off,mapnoname,mapradio,' +
                'chtag,source,services_count,services_seen,comment';
    var list  = 'enabled,rescan,' + list0;
    var elist = 'enabled,rescan,ext_url,' + list0;
    var alist = 'enabled,ext_url,' + list0;

    tvheadend.idnode_grid(panel, {
        url: 'api/bouquet',
        titleS: _('Bouquet'),
        titleP: _('Bouquets'),
        iconCls: 'bouquets',
        tabIndex: index,
        columns: {
            enabled:        { width: 50 },
            rescan:         { width: 50 },
            name:           { width: 200 },
            maptoch:        { width: 100 },
            mapnolcn:       { width: 100 },
            mapradio:       { width: 100 },
            lcn_off:        { width: 100 },
            mapnoname:      { width: 100 },
            chtag:          { width: 100 },
            source:         { width: 200 },
            services_count: { width: 100 },
            services_seen:  { width: 100 },
            comment:        { width: 200 }
        },
        list: list,
        add: {
            url: 'api/bouquet',
            params: { list: alist },
            create: { }
        },
        del: true,
        edit: { params: { list: elist } },
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help(_('Bouquets'), 'config_bouquet.html');
        }
    });

    return panel;
};
