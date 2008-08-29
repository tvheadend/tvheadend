

/**
 * DVB adapter details
 */
tvheadend.dvb_adapterdetails = function(adapterId, adapterName, treenode) {

    var confreader = new Ext.data.JsonReader({
	    root: 'dvbadapters',
	}, ['name', 'automux']);

 
    
    

    function addmux() {

	var locationbutton = new Ext.Button({
		text: 'Add',
		disabled: true,
		handler: function() {
		    var n = locationlist.getSelectionModel().getSelectedNode();
		    Ext.Ajax.request({url: '/dvbadapter',
				params: {network: n.attributes.id,
				    adapterId: adapterId, op: 'addnetwork'}});
		    win.close();
		}
	    });

	var locationlist = new Ext.tree.TreePanel({
	    title:'By location',
	    autoScroll:true,
	    rootVisible:false,
	    loader: new Ext.tree.TreeLoader({
		    baseParams: {adapter: adapterId},
		    dataUrl:'/dvbnetworks',
		}),
	    
	    root: new Ext.tree.AsyncTreeNode({
		    id:'root',
		}),

	    buttons: [locationbutton],
	    buttonAlign: 'center'
	    });


	locationlist.on('click', function(n) {
		if(n.attributes.leaf) {
		    locationbutton.enable();
		} else {
		    locationbutton.disable();
		}
	    });

  
	win = new Ext.Window({
		title: 'Add mux(es) on ' + adapterName,
                layout: 'fit',
                width: 500,
                height: 500,
		modal: true,
                plain: true,
                items: new Ext.TabPanel({
                    autoTabs: true,
                    activeTab: 0,
                    deferredRender: false,
                    border: false,
		    items: [locationlist, {
				html: 'def',
				title: 'Manual configuration'
			    }]
                })
            });
	win.show();
    }

    /**
     *
     */
   function probeservices() {
	Ext.MessageBox.confirm('Message',
			       'Probe all DVB services on "' + adapterName + 
			       '" and map to TV-channels in tvheadend',
			       function(button) {
				   if(button == 'no')
				       return;

				   Ext.Ajax.request({url: '/dvbadapter',
					       params: {adapterId: adapterId, 
						   op: 'serviceprobe'}
				       })
			       });
   };


    /**
     *
     */
    function saveconfig() {
	panel.getForm().submit({url:'/dvbadapter', 
		    params:{'adapterId': adapterId, 'op':'save'},
		    waitMsg:'Saving Data...'
		    });
    };

    

   var panel = new Ext.FormPanel({
	    border:false,
	    disabled:true,
	    title: adapterName,
	    bodyStyle:'padding:15px',
	    labelAlign: 'right',
	    labelWidth: 150,
	    waitMsgTarget: true,
	    reader: confreader,
	    defaultType: 'textfield',
	    items: [{
		    fieldLabel: 'Adapter name',
		    name: 'name',
		    width: 400,
		},
		
		new Ext.form.Checkbox({
			fieldLabel: 'Autodetect muxes',
			name: 'automux',
		    })
		],
	    tbar:[{
		    tooltip: 'Manually add new transport multiplexes',
		    iconCls:'add',
		    text: 'Add mux(es)',
		    handler: addmux
		}, '-', {
		    tooltip: 'Scan all transports on this adapter and map those who has a working video stream to a channel',
		    iconCls:'option',
		    text: 'Probe services',
		    handler: probeservices
		}, '-', {
		   tooltip: 'Save and changes made to the configuation below',
		    iconCls:'save',
		    text: 'Save configuration',
		    handler: saveconfig
		}]
	});
    
    panel.getForm().load({url:'/dvbadapter', 
		params:{'adapterId': adapterId, 'op':'load'},
		success:function(form, action) {
		panel.enable();
	    }});
    

    return panel;
}


/**
 *
 */
tvheadend.dvb = function() {

 
    var tree = new Ext.tree.ColumnTree({
	    region:'west',
	    autoScroll:true,
	    rootVisible:false,
    
	    columns:[{
		    header:'Name',
		    width:300,
		    dataIndex:'name'
		},{
		    header:'Type',
		    width:100,
		    dataIndex:'type'
		},{
		    header:'Status',
		    width:100,
		    dataIndex:'status'
		},{
		    header:'Quality',
		    width:100,
		    dataIndex:'quality'
		}],
		  
	    loader: new Ext.tree.TreeLoader({
		    clearOnLoad: true,
		    dataUrl:'/dvbtree',
		    uiProviders:{
			'col': Ext.tree.ColumnNodeUI
		    }
		}),

	    root: new Ext.tree.AsyncTreeNode({
		    id:'root',
		}),
	});


    /**
     *
     */

    var details = new Ext.Panel({
	    region:'center', layout:'fit', 
	    items:[{border: false}]
	});
    
    /**
     *
     */
    var panel = new Ext.Panel({
	    border: false,
	    title:'DVB Adapters',
	    layout:'border',
	    items: [tree, details]
	});

    /**
     *
     */
    tree.on('click', function(n) {
	details.remove(details.getComponent(0));

	details.doLayout();

	switch(n.attributes.itype) {
	case 'adapter':
	    var newPanel = 
		new tvheadend.dvb_adapterdetails(n.attributes.id,
						 n.attributes.name, n);
	    break;
	    
	case 'mux':
	case 'transport':
	default:
	    var newPanel = {title: n.attributes.name, html: ''};
	    break;
	}

	details.add(newPanel);
	details.doLayout();
    });

    tvheadend.dvbtree = tree;

    return panel;
}
