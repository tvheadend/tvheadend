
tvheadend.cwceditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 60
    });

    var cm = new Ext.grid.ColumnModel([
				       enabledColumn,
	{
	    id:'hostname',
	    header: "Hostname",
	    dataIndex: 'hostname',
	    width: 200,
	    editor: new fm.TextField({
               allowBlank: false
			       })
	},{
	    id:'port',
	    header: "Port",
	    dataIndex: 'port',
	    editor: new fm.TextField({
               allowBlank: false
			       })
	},{
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
	    header: "DES Key",
	    dataIndex: 'deskey',
	    width: 300,
	    editor: new fm.TextField({
               allowBlank: false,
			       })
	},
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
       {name: 'hostname'},
       {name: 'port'},
       {name: 'username'},
       {name: 'password'},
       {name: 'deskey'}, 
       {name: 'comment'}]);
 
    var store = new Ext.data.JsonStore({root: 'entries',
						  fields: UserRecord,
						  url: "tablemgr",
						  autoLoad: true,
						  id: 'id',
						  storeid: 'id',
						  baseParams: {table: "cwc", op: "get"}
    });
    

    function addRecord() {
	Ext.Ajax.request({
	    url: "tablemgr", params: {op:"create", table:"cwc"},
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
		    url: "tablemgr", params: {op:"delete", table:"cwc",
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
			  params: {op:"update", table:"cwc",
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
	    title: 'Code Word Client',
	    plugins:[enabledColumn],
	    store: store,
	    clicksToEdit: 2,
	    cm: cm,
	    selModel: new Ext.grid.RowSelectionModel({singleSelect:false}),
	    tbar: [{
		    tooltip: 'Create a new code word connection entry on the server. ' + 
		    'The new entry is initially disabled so it must be enabled before it start taking effect.',
		    iconCls:'add',
		    text: 'Add entry',
		    handler: addRecord
		}, '-', {
		    tooltip: 'Delete one or more selected rows',
		    iconCls:'remove',
		    text: 'Delete selected',
		    handler: delSelected
		}, '-', {
		    tooltip: 'Save any changes made (Changed cells have red borders).',
		    iconCls:'save',
		    text: "Save changes",
		    handler: saveChanges
		}
		]
	});
    return grid;
}
