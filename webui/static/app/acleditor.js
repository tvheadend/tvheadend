
tvheadend.acleditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 60
    });

    var streamingColumn = new Ext.grid.CheckColumn({
       header: "Streaming",
       dataIndex: 'streaming',
       width: 100
    });

    var pvrColumn = new Ext.grid.CheckColumn({
       header: "Video Recorder",
       dataIndex: 'pvr',
       width: 100
    });

    var webuiColumn = new Ext.grid.CheckColumn({
       header: "Web Interface",
       dataIndex: 'webui',
       width: 100
    });

    var adminColumn = new Ext.grid.CheckColumn({
       header: "Admin",
       dataIndex: 'admin',
       width: 100
    });

    var cm = new Ext.grid.ColumnModel([
				       enabledColumn,
	{
	    id:'username',
	    header: "Username",
	    dataIndex: 'username',
	    editor: new fm.TextField({
               allowBlank: false
			       })
	},{
	    id:'password',
	    header: "Password",
	    dataIndex: 'password',
	    editor: new fm.TextField({
               allowBlank: false
			       })
	},{
	    header: "Prefix",
	    dataIndex: 'prefix',
	    editor: new fm.TextField({
               allowBlank: false,
			       })
	},
	    streamingColumn,
	    pvrColumn,
	    webuiColumn,
	    adminColumn,

	{
	    id:'comment',
	    header: "Comment",
	    dataIndex: 'comment',
	    width: 400,
	    editor: new fm.TextField({
               
			       })
	}
	]);
    
    
    cm.defaultSortable = true;
    
    var UserRecord = Ext.data.Record.create([
    {name: 'enabled'},
    {name: 'streaming'},
    {name: 'pvr'},
    {name: 'admin'},
    {name: 'webui'},
    {name: 'username'},
    {name: 'prefix'},
    {name: 'password'},
    {name: 'comment'}]);
 
    var store = new Ext.data.JsonStore({root: 'entries',
					fields: UserRecord,
					url: "tablemgr",
					autoLoad: true,
					id: 'id',
					storeid: 'id',
					baseParams: {table: "accesscontrol", op: "get"}
	});
    

    function addRecord() {
	Ext.Ajax.request({
		url: "tablemgr", params: {op:"create", table:"accesscontrol"},
		    failure:function(response,options){
		    Ext.MessageBox.alert('Server Error','Unable to generate new record');
		},
		    success:function(response,options){
		    var responseData = Ext.util.JSON.decode(response.responseText);
		    
		    var p = new UserRecord(responseData, responseData.id);
		    grid.stopEditing();
		    store.insert(0, p);
		    grid.startEditing(0, 0);
		}
	    })
	    };
    
    function delSelected() {
	var selectedKeys = grid.selModel.selections.keys;
	if(selectedKeys.length > 0)
            {
                Ext.MessageBox.confirm('Message',
				       'Do you really want to delete selection?',
				       deleteRecord);
            } else {
                Ext.MessageBox.alert('Message',
				     'Please select at least one item to delete');
            }
    };
    
    
    function deleteRecord(btn) {
	if(btn=='yes') {
	    var selectedKeys = grid.selModel.selections.keys;
	    
	    Ext.Ajax.request({
		    url: "tablemgr", params: {op:"delete", table:"accesscontrol",
			    entries:Ext.encode(selectedKeys)},
			failure:function(response,options){
			Ext.MessageBox.alert('Server Error','Unable to delete');
		    },
			success:function(response,options){
			store.reload();
		    }
		})
		}
    }
 


    function saveChanges() {
	var mr = store.getModifiedRecords();
	var out = new Array();
	for (var x = 0; x < mr.length; x++) {
	    v = mr[x].getChanges();
	    out[x] = v;
	    out[x].id = mr[x].id;
	}

	Ext.Ajax.request({url: "tablemgr",
			  params: {op:"update", table:"accesscontrol",
				   entries:Ext.encode(out)},
			  success:function(response,options){
		    store.commitChanges();
		},
			  failure:function(response,options){
		    Ext.MessageBox.alert('Message',response.statusText);
		}
	    });
    }
       
    var grid = new Ext.grid.EditorGridPanel({
	    title: 'Access Configuration',
	    plugins:[enabledColumn,streamingColumn,pvrColumn,adminColumn, webuiColumn],
	    store: store,
	    clicksToEdit: 2,
	    cm: cm,
	    selModel: new Ext.grid.RowSelectionModel({singleSelect:false}),
	    tbar: [{
		    iconCls:'add',
		    text: 'Add entry',
		    handler: addRecord
		}, '-', {
		    iconCls:'remove',
		    text: 'Delete selected',
		    handler: delSelected
		}, '-', {
		    iconCls:'save',
		    text: "Save changes",
		    handler: saveChanges
		}
		]
	});
    
    return grid;
}
