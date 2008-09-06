
tvheadend.cteditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
	header: "Enabled",
	dataIndex: 'enabled',
	width: 60
    });

    var internalColumn = new Ext.grid.CheckColumn({
       header: "Internal",
       dataIndex: 'internal',
       width: 100
    });


    var cm = new Ext.grid.ColumnModel([
				       enabledColumn,
	{
	    id:'name',
	    header: "Name",
	    dataIndex: 'name',
	    editor: new fm.TextField({
               allowBlank: false
			       })
	},
	    internalColumn,
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
    
    var ChannelTagRecord = Ext.data.Record.create([
	{name: 'enabled'},
	{name: 'name'},
	{name: 'internal'},
	{name: 'comment'}
    ]);
    
    var store = new Ext.data.JsonStore({
	root: 'entries',
	fields: ChannelTagRecord,
	url: "tablemgr",
	autoLoad: true,
	id: 'id',
	storeid: 'id',
	baseParams: {table: "channeltags", op: "get"}
    });
    

    function addRecord() {
	Ext.Ajax.request({
		url: "tablemgr", params: {op:"create", table:"channeltags"},
		    failure:function(response,options){
		    Ext.MessageBox.alert('Server Error','Unable to generate new record');
		},
		    success:function(response,options){
		    var responseData = Ext.util.JSON.decode(response.responseText);
		    
		    var p = new ChannelTagRecord(responseData, responseData.id);
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
		    url: "tablemgr", params: {op:"delete", table:"channeltags",
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
			  params: {op:"update", table:"channeltags",
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
	    title: 'Channel Tags',
	    plugins:[enabledColumn,internalColumn],
	    store: store,
	    clicksToEdit: 2,
	    cm: cm,
	    selModel: new Ext.grid.RowSelectionModel({singleSelect:false}),
	    tbar: [{
		    tooltip: 'Create a new channel tag entry on the server. ' + 
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
