tvheadend.acleditor = function() {
	var fm = Ext.form;

  var cm = new Ext.grid.ColumnModel({
    defaultSortable: true,

    columns : [{
      xtype: 'checkcolumn',
      header : "Enabled",
      dataIndex : 'enabled',
      width : 60
    }, {
      header : "Username",
      dataIndex : 'username',
      editor : new fm.TextField({
	allowBlank : false
      })
    }, {
      header : "Password",
      dataIndex : 'password',
      renderer : function(value, metadata, record, row, col, store) {
	return '<span class="tvh-grid-unset">Hidden</span>';
      },
      editor : new fm.TextField({
	allowBlank : false
      })
    }, {
      header : "Prefix",
      dataIndex : 'prefix',
      editor : new fm.TextField({
	allowBlank : false
      })
    }, {
      xtype: 'checkcolumn',
      header : "Streaming",
      dataIndex : 'streaming',
      width : 100
    }, {
      xtype: 'checkcolumn',
      header : "Video Recorder",
      dataIndex : 'dvr',
      width : 100
    }, {
      xtype: 'checkcolumn',
      header : "All Configs (VR)",
      dataIndex : 'dvrallcfg',
      width : 100
    }, {
      xtype: 'checkcolumn',
      header : "Web Interface",
      dataIndex : 'webui',
      width : 100
    }, {
      xtype: 'checkcolumn',
      header : "Admin",
      dataIndex : 'admin',
      width : 100
    }, {
      xtype: 'checkcolumn',
      header : "Access Tag Only",
      dataIndex : 'tag_only',
      width : 200
    }, {      
      header : "Comment",
      dataIndex : 'comment',
      width : 300,
      editor : new fm.TextField({})
    }]});

  var UserRecord = Ext.data.Record.create(
    [ 'enabled', 'streaming', 'dvr', 'dvrallcfg', 'admin', 'webui', 'username', 'tag_only',
      'prefix', 'password', 'comment'
    ]);

  return new tvheadend.tableEditor('Access control', 'accesscontrol', cm,
		                   UserRecord, [], null, 'config_access.html',
                                   'group');
}
