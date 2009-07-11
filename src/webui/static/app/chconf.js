
/**
 * Channel tags
 */
tvheadend.channelTags = new Ext.data.JsonStore({
    autoLoad:true,
    root:'entries',
    fields: ['identifier', 'name'],
    id: 'identifier',
    url:'channeltags',
    baseParams: {op: 'listTags'}
});

tvheadend.comet.on('channeltags', function(m) {
    if(m.reload != null)
        tvheadend.channelTags.reload();
});


/**
 * Channels
 */
tvheadend.channels = new Ext.data.JsonStore({
    autoLoad: true,
    root:'entries',
    fields: ['name', 'chid'],
    id: 'chid',
    url: "chlist"
});

tvheadend.comet.on('channels', function(m) {
    if(m.reload != null)
        tvheadend.channels.reload();
});


/**
 * Channel details
 */
tvheadend.channeldetails = function(chid, chname) {
    var fm = Ext.form;
    var xg = Ext.grid;

    var expander = new xg.RowExpander({
        tpl : new Ext.Template(
            '<div><b width=100px>Video:</b>{video}</div>',
            '<div><b>Audio:</b>{audio}</div>',
            '<div><b>Subtitling:</b>{subtitles}</div>',
            '<div><b>Scrambling:</b>{scrambling}</div>'
        )
    });
    
    var enabledColumn = new Ext.grid.CheckColumn({
	header: "Enabled",
	dataIndex: 'enabled',
	width: 60
    });

    var cm = new Ext.grid.ColumnModel([expander,
	enabledColumn,
	{
	    width: 125,
	    id:'name',
	    header: "Original name",
	    dataIndex: 'name'
	},{
	    width: 125,
	    id:'status',
	    header: "Last status",
	    dataIndex: 'status'
	},{
	    width: 125,
	    id:'provider',
	    header: "Provider",
	    dataIndex: 'provider'
	},{
	    width: 125,
	    id:'network',
	    header: "Network",
	    dataIndex: 'network'
	},{
	    width: 250,
	    id:'source',
	    header: "Source",
	    dataIndex: 'source'
	}
				      ]);
    
    
    var transportRecord = Ext.data.Record.create([
	{name: 'enabled'}, 
	{name: 'status'},
	{name: 'name'},
	{name: 'provider'},
	{name: 'network'},
	{name: 'source'},
	{name: 'video'},
	{name: 'audio'},
	{name: 'scrambling'},
	{name: 'subtitles'}
    ]);
    
    var transportsstore =
	new Ext.data.JsonStore({root: 'entries',
				fields: transportRecord,
				url: "channel",
				autoLoad: true,
				id: 'id',
				storeid: 'id',
				baseParams: {chid: chid, op: "gettransports"}
			       });
    

    var transportsgrid = new Ext.grid.EditorGridPanel({
	title:'Transports',
	anchor: '100% 50%',
	stripeRows:true,
	plugins:[enabledColumn, expander],
	store: transportsstore,
	clicksToEdit: 2,
        viewConfig: {forceFit:true},
	cm: cm,
	selModel: new Ext.grid.RowSelectionModel({singleSelect:false})
    });


    var confreader = new Ext.data.JsonReader({
	root: 'channels'
    }, ['name','xmltvchannel','tags']);

    
    var xmltvChannels = new Ext.data.JsonStore({
	root:'entries',
	fields: [{name: 'xcTitle'}, {name: 'xcIcon'}],
	url:'xmltv',
	baseParams: {op: 'listChannels'}
    });

 
    var confpanel = new Ext.FormPanel({
	border:false,
	disabled:true,
	bodyStyle:'padding:15px',
	anchor: '100% 50%',
	labelAlign: 'right',
	labelWidth: 150,
	waitMsgTarget: true,
	reader: confreader,

	items: [{
	    layout:'column',
	    border:false,
	    items:[{
		border:false,
		columnWidth:.5,
		layout: 'form',
		defaultType: 'textfield',
		items: [
		    {
			fieldLabel: 'Channel name',
			name: 'name'
		    },new Ext.form.ComboBox({
			loadingText: 'Loading...',
			fieldLabel: 'XML-TV Source',
			name: 'xmltvchannel',
			width: 200,
			displayField:'xcTitle',
			valueField:'xcTitle',
			store: xmltvChannels,
			forceSelection: true,
			mode: 'remote',
			editable: false,
			triggerAction: 'all',
			emptyText: 'None'
		    })
		]
	    },{
		border:false,
		columnWidth:.5,
		layout: 'form',
		items: [{
		    fieldLabel: 'Tags',
		    xtype:"multiselect",
		    name:"tags",
		    valueField:"identifier",
		    displayField:"name",
		    width:200,
		    height:200,
		    store:tvheadend.channelTags
		}]
	    }]
	}]
    });

    confpanel.getForm().load({url:'channel', 
			      params:{'chid': chid, 'op':'load'},
			      success:function(form, action) {
				  confpanel.enable();
			      }});


    function saveChanges() {
	confpanel.getForm().submit({url:'channel', 
				    params:{'chid': chid, 'op':'save'},
				    waitMsg:'Saving Data...',
				    failure: function(form, action) {
					Ext.Msg.alert('Save failed', action.result.errormsg);
				    }
				   });
    }

    function deleteChannel() {
	Ext.MessageBox.confirm('Message',
			       'Do you really want to delete "' + chname + '"',
			       function(button) {
				   if(button == 'no')
				       return;
				   Ext.Ajax.request({url: 'channel',
						     params:{'chid': chid, 'op':'delete'},
						     success: function() {
							 panel.destroy();
						     }
						    });
			       }
			      );
    }

    var panel = new Ext.Panel({
	title: chname,
	border:false,
	tbar: [{
	    tooltip: 'Delete channel "' + chname + '". All mapped transports will be unmapped',
	    iconCls:'remove',
	    text: "Delete channel",
	    handler: deleteChannel
	}, '-', {
	    tooltip: 'Save changes made to channel configuration below and the mapped transports',
	    iconCls:'save',
	    text: "Save configuration",
	    handler: saveChanges
	}, '->', {
	    text: 'Help',
	    handler: function() {
		new tvheadend.help('Channel configuration', 
				   'config_channels.html');
	    }
	}],
	defaults: {
	    border:false
	},
	layout:'anchor',
	items: [confpanel,transportsgrid]
    });


    panel.on('afterlayout', function(parent, n) {
	var DropTargetEl = parent.body.dom;
	
	var DropTarget = new Ext.dd.DropTarget(DropTargetEl, {
	    ddGroup     : 'chconfddgroup',
	    notifyEnter : function(ddSource, e, data) {
		
		//Add some flare to invite drop.
		parent.body.stopFx();
		parent.body.highlight();
	    },
	    notifyDrop  : function(ddSource, e, data){
		
		// Reference the record (single selection) for readability
		var selectedRecord = ddSource.dragData.selections[0];						

		Ext.MessageBox.confirm('Merge channels',
				       'Copy transport configuration from "' + selectedRecord.data.name +
				       '" to "' + chname + '". This will also remove the channel "' +
				       selectedRecord.data.name + '"',
				       function(button) {
					   if(button == 'no')
					       return;
					   Ext.Ajax.request({url: 'channel',
							     params:{chid: chid, 
								     op:'mergefrom', 
								     srcch: selectedRecord.data.chid},
							     success: function() {
								 transportsstore.reload();
							     }});
				       }
				      );
	    }
	}); 	
    });
    return panel;
}


/**
 *
 */
tvheadend.chconf = function() {
    var chlist = new Ext.grid.GridPanel({
        viewConfig: {forceFit:true},
	ddGroup: 'chconfddgroup',
	enableDragDrop: true,
	stripeRows:true,
	region:'west',
	width: 300,
	columns: [{id:'name',
		   header: "Channel name",
		   width: 260,
		   dataIndex: 'name'}
		 ],
	selModel: new Ext.grid.RowSelectionModel({singleSelect:true}),
	store: tvheadend.channels
    });

    var details = new Ext.Panel({
	region:'center', layout:'fit', 
	items:[{border: false}]
    });


    var panel = new Ext.Panel({
	border: false,
	title:'Channels',
	layout:'border',
	items: [chlist, details]
    });


    chlist.on('rowclick', function(grid, n) {
	var rec = tvheadend.channels.getAt(n);
	
	details.remove(details.getComponent(0));
	details.doLayout();

	var newpanel = new tvheadend.channeldetails(rec.data.chid,
	    rec.data.name);
	
	details.add(newpanel);
	details.doLayout();
    });


    /**
     * Setup Drop Targets
     */
    
    // This will make sure we only drop to the view container

    /*
    var DropTargetEl = details.getView();

    var DropTarget = new Ext.dd.DropTarget(DropTargetEl, {
	    ddGroup     : 'chconfddgroup',
	    notifyEnter : function(ddSource, e, data) {
		
		//Add some flare to invite drop.
		panel.body.stopFx();
		panel.body.highlight();
	    },
	    notifyDrop  : function(ddSource, e, data){
			
		// Reference the record (single selection) for readability
		var selectedRecord = ddSource.dragData.selections[0];						
		
		console.log(selectedRecord);
	    }
	}); 	

    */
    /*
    details.on('afterlayout', function(parent, n) {
	    console.log(parent);
	});
    */
    return panel;
}
