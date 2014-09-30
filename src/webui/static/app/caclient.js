/*
 * Conditional Access Client (cwc,capmt)
 */

tvheadend.caclient_builders = new Ext.data.JsonStore({
  url: 'api/caclient/builders',
  root: 'entries',
  fields: ['class', 'caption', 'props'],
  id: 'class',
  autoLoad: true
});

tvheadend.caclient = function(panel, index) {
    var list = 'enabled,name,username,password,hostname,mode,camdfilename,' +
               'port,deskey,emm,emmex,comment';

    tvheadend.idnode_form_grid(panel, {
        url: 'api/caclient',
        clazz: 'caclient',
        comet: 'caclient',
        titleS: 'CA',
        titleP: 'CAs',
        titleC: 'Client Name',
        iconCls: 'key',
        tabIndex: index,
        list: { url: 'api/caclient/list', params: { } },
        edit: { params: { list: list } },
        add: {
            url: 'api/caclient',
            titleS: 'Conditional Access Client',
            select: {
                label: 'Type',
                store: tvheadend.caclient_builders,
                displayField: 'caption',
                valueField: 'class',
                propField: 'props',
                list: list
            },
            create: { },
        },
        del: true,
        move: true,
        help: function() {
            new tvheadend.help('Conditional Access Client', 'config_caclient.html');
        },
    });

    return panel;

}
