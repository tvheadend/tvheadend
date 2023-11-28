tvheadend.ratinglabel = function(panel, index) {

    tvheadend.idnode_grid(panel, {
        url: 'api/ratinglabel',
        all: 1,
        titleS: _('Rating Label'),
        titleP: _('Rating Labels'),
        iconCls: 'baseconf',
        tabIndex: index,
        columns: {
            enabled:        { width: 70 },
            country:        { width: 80 },
            age:            { width: 50 },
            display_age:    { width: 100 },
            display_label:  { width: 100 },
            label:          { width: 100 },
            authority:      { width: 100 },
            icon:           { width: 200 }
        },
        add: {
            url: 'api/ratinglabel',
            create: { }
        },
        del: true,
        uilevel: 'expert',
        del: true,
        sort: {
          field: 'country',
          direction: 'ASC'
        }
    });

    return panel;

}
