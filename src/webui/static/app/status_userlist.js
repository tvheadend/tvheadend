tvheadend.statusUserlist = new Ext.data.JsonStore({
        root : 'entries',
        id : 'id',
        url : '/status/userlist',
        baseParams : {
                op : 'streamdata'
        },
        fields : [ 'id', 'username', 'startlog', 'currlog', 'ip', 'type', 'streamdata' ],
        autoLoad : true
});

tvheadend.statusUserlist.setDefaultSort('username', 'ASC');
tvheadend.comet.on('statususerlist', function(m) {
        if (m.reload != null) tvheadend.statusUserlist.reload();
});


tvheadend.statususerlist = function() {
	var fm = Ext.form;

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true,
  columns : [ {
                header : "ID",
                dataIndex : 'uid',
		width : 10,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "Username",
                dataIndex : 'username',
		width : 30,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "Startlog",
                dataIndex : 'startlog',
		width : 40,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "Currlog",
                dataIndex : 'currlog',
		width : 40,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "IP",
                dataIndex : 'ip',
		width : 40,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "Type",
                dataIndex : 'type',
		width : 40,
                editor : new fm.TextField({
                        allowBlank : false
                })
        }, {
                header : "Streamdata",
                dataIndex : 'streamdata',
		width : 40,
                editor : new fm.TextField({
                        allowBlank : false
                })
	} ]});

	var UserStatusRecord = Ext.data.Record.create([ 'id', 'username',
		'startlog', 'currlog', 'ip', 'type', 'streamdata' ]);

	return new tvheadend.tableEditor('Status', 'statususerlist', cm,
		UserStatusRecord, null ,
		null, 'config_userlist.html', 'eye');

}
