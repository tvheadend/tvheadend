tvheadend.cteditor = function() {
	var fm = Ext.form;

	var enabledColumn = new Ext.grid.CheckColumn({
		header : "Enabled",
		dataIndex : 'enabled',
		width : 60
	});

	var internalColumn = new Ext.grid.CheckColumn({
		header : "Internal",
		dataIndex : 'internal',
		width : 100
	});

	var titledIconColumn = new Ext.grid.CheckColumn({
		header : "Icon has title",
		dataIndex : 'titledIcon',
		width : 100,
		tooltip : 'Set this if the supplied icon has a title embedded. '
			+ 'This will tell displaying application not to superimpose title '
			+ 'on top of logo.'
	});

	var cm = new Ext.grid.ColumnModel({
  defaultSortable: true,
  columns : [ enabledColumn, {
		header : "Name",
		dataIndex : 'name',
		editor : new fm.TextField({
			allowBlank : false
		})
	}, internalColumn, {
		header : "Icon (full URL)",
		dataIndex : 'icon',
		width : 400,
		editor : new fm.TextField({})
	}, titledIconColumn, {
		header : "Comment",
		dataIndex : 'comment',
		width : 400,
		editor : new fm.TextField({})
	} ]});

	var ChannelTagRecord = Ext.data.Record.create([ 'enabled', 'name',
		'internal', 'icon', 'comment', 'titledIcon' ]);

	return new tvheadend.tableEditor('Channel Tags', 'channeltags', cm,
		ChannelTagRecord, [ enabledColumn, internalColumn, titledIconColumn ],
		null, 'config_tags.html', 'tags');
}
