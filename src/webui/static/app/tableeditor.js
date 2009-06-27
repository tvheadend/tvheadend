tvheadend.tableEditor = function(title, dtable, cm, rec, plugins, store,
				 helpContent) {
    cm.defaultSortable = true;

    if(store == null) {
	store = new Ext.data.JsonStore({
	    root: 'entries',
	    fields: rec,
	    url: "tablemgr",
	    autoLoad: true,
	    id: 'id',
	    baseParams: {table: dtable, op: "get"}
	});
    }

    function addRecord() {
	Ext.Ajax.request({
	    url: "tablemgr", params: {op:"create", table:dtable},
	    failure:function(response,options){
		Ext.MessageBox.alert('Server Error',
				     'Unable to generate new record');
	    },
	    success:function(response,options){
		var responseData = Ext.util.JSON.decode(response.responseText);
		var p = new rec(responseData, responseData.id);
		grid.stopEditing();
		store.insert(0, p);
		grid.startEditing(0, 0);
	    }
	})
    };
    
    
    
    function delSelected() {
	var selectedKeys = grid.selModel.selections.keys;
	if(selectedKeys.length > 0) {
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
		url: "tablemgr", params: {op:"delete", table:dtable,
					  entries:Ext.encode(selectedKeys)},
		failure:function(response,options) {
		    Ext.MessageBox.alert('Server Error','Unable to delete');
		},
		success:function(response,options) {
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

	Ext.Ajax.request({
	    url: "tablemgr",
	    params: {op:"update", table:dtable, entries:Ext.encode(out)},
	    success:function(response,options) {
		store.commitChanges();
	    },
	    failure:function(response,options) {
		Ext.MessageBox.alert('Message',response.statusText);
	    }
	});
    }
       
    var selModel = new Ext.grid.RowSelectionModel({
	singleSelect:false
    });

    var delButton = new Ext.Toolbar.Button({
	tooltip: 'Delete one or more selected rows',
	iconCls:'remove',
	text: 'Delete selected',
	handler: delSelected,
	disabled: true
    });

    selModel.on('selectionchange', function(self) {
	if(self.getCount() > 0) {
	    delButton.enable();
	} else {
	    delButton.disable();
	}

	console.log('tut');
    });

    var grid = new Ext.grid.EditorGridPanel({
	title: title,
	plugins: plugins,
	store: store,
	clicksToEdit: 2,
	cm: cm,
        viewConfig: {forceFit:true},
	selModel: selModel,
	tbar: [{
	    tooltip: 'Create a new entry on the server. '+
		'The new entry is initially disabled so it must be enabled '+
		'before it start taking effect.',
	    iconCls:'add',
	    text: 'Add entry',
	    handler: addRecord
	}, '-', delButton, '-', {
	    tooltip: 'Save any changes made (Changed cells have red borders).',
	    iconCls:'save',
	    text: "Save changes",
	    handler: saveChanges
	}, '->', {
	    text: 'Help',
	    handler: function() {
		new tvheadend.help(title, helpContent);
	    }
	}
	      ]
    });
    return grid;
}

