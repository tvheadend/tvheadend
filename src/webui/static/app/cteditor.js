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
        }
    });

    return panel;
};

/*
 * Bouquet
 */
tvheadend.bouquet = function(panel, index)
{
    var list0 = 'name,maptoch,lcn_off,mapopt,chtag,source,services_count,services_seen,comment';
    var list  = 'enabled,rescan,' + list0;
    var elist = 'enabled,rescan,ext_url,' + list0;
    var alist = 'enabled,ext_url,' + list0;

    var scanButton = {
        name: 'scan',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Rescan the mux for changes to the bouquet.'),
                iconCls: 'find',
                text: _('Force Scan'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/bouquet/scan',
                    params: {
                        uuid: Ext.encode(uuids)
                    },    
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };

    function selected(s, abuttons) {
        abuttons.scan.setDisabled(!s || s.length <= 0);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/bouquet',
        titleS: _('Bouquet'),
        titleP: _('Bouquets'),
        iconCls: 'bouquets',
        tabIndex: index,
        tbar: [scanButton],
        columns: {
            enabled:        { width: 50 },
            rescan:         { width: 50 },
            name:           { width: 200 },
            lcn_off:        { width: 100 },
            mapopt:         { width: 150 },
            chtag:          { width: 150 },
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
        selected: selected,
        sort: {
          field: 'name',
          direction: 'ASC'
        }
    });

    return panel;
};
