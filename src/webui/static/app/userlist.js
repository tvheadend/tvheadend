tvheadend.servicetypeStore = new Ext.data.JsonStore({
        root : 'entries',
        id : 'val',
        url : '/status/userlist',
        baseParams : {
                op : 'streamdata'
        },
        fields : [ 'val', 'str' ],
        autoLoad : true
});

tvheadend.userlist = function() {
	var fm = Ext.form;

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true
	});

	var UserStatusRecord = Ext.data.Record.create([ 'id', 'username',
		'startlog', 'currlog', 'ip', 'type', 'streamdata' ]);

	return new tvheadend.tableEditor('Status', 'userlist', cm,
		UserStatusRecord, null ,
		null, 'config_userlist.html', 'users');
}
