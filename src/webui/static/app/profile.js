/*
 * Stream Profiles
 */

tvheadend.profile_tab = function(panel)
{
    if (!tvheadend.profile_builders) {
        tvheadend.profile_builders = new Ext.data.JsonStore({
            url: 'api/profile/builders',
            root: 'entries',
            fields: ['class', 'caption', 'order', 'groups', 'params'],
            id: 'class',
            autoLoad: true
        });
    }

    var list = '-class';

    tvheadend.idnode_form_grid(panel, {
        url: 'api/profile',
        clazz: 'profile',
        comet: 'profile',
        titleS: _('Stream Profile'),
        titleP: _('Stream Profiles'),
        titleC: _('Stream Profile Name'),
        iconCls: 'film',
        edit: { params: { list: list } },
        add: {
            url: 'api/profile',
            titleS: _('Stream Profile'),
            select: {
                label: _('Type'),
                store: tvheadend.profile_builders,
                fullRecord: true,
                displayField: 'caption',
                valueField: 'class',
                list: list
            },
            create: { }
        },
        del: true
    });
};
