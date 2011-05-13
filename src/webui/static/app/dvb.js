

/**
 * DVB Mux grid
 */
tvheadend.dvb_muxes = function(adapterData, satConfStore) {

    adapterId = adapterData.identifier;

    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 40
    });

    var qualityColumn = new Ext.ux.grid.ProgressColumn({
	header : "Quality",
	dataIndex : 'quality',
	width : 85,
	textPst : '%',
	colored : true
    });

    var cmlist = Array();

    cmlist.push(enabledColumn,
	{
	    header: "Network",
	    dataIndex: 'network',
	    width: 200
	},
	{
	    header: "Frequency",
	    dataIndex: 'freq',
	    width: 50
	},
	{
	    header: "Modulation",
	    dataIndex: 'mod',
	    width: 100
	});

    if(adapterData.satConf) {
	// Include DVB-S specific stuff

	satConfStore.on('update', function(s, r, c) {
	    if(grid.rendered)
		grid.getView().refresh();
	});

	satConfStore.on('load', function(s, r, o) {
	    if(grid.rendered)
		grid.getView().refresh();
	});

	tvheadend.comet.on('dvbSatConf', function(m) {
	    if(m.adapterId == adapterId)
		satConfStore.reload();
	});

	cmlist.push({
	    header: "Polarisation",
	    dataIndex: 'pol',
	    width: 50
	}, {
	    header: "Satellite config",
	    dataIndex: 'satconf',
	    width: 100,
	    renderer: function(value, metadata, record, row, col, store) {
		r = satConfStore.getById(value);
		return typeof r === 'undefined' ? 
		    '<span class="tvh-grid-unset">Unset</span>'
		    : r.data.name;
	    }
	}, {
      header: "Frontend status",
      dataIndex: 'fe_status',
      width: 30
  });
    }

    cmlist.push(
	{
	    header: "MuxID",
	    dataIndex: 'muxid',
	    width: 50
	},
	qualityColumn
    );

    var cm = new Ext.grid.ColumnModel(cmlist);
    cm.defaultSortable = true;

    var rec = Ext.data.Record.create([
	'id', 'enabled','network', 'freq', 'pol', 'satconf', 
	'muxid', 'quality', 'fe_status', 'mod'
    ]);

    var store = new Ext.data.JsonStore({
	root: 'entries',
	fields: rec,
	url: "dvb/muxes/" + adapterId,
	autoLoad: true,
	id: 'id',
	baseParams: {op: "get"},
	listeners: {
	    'update': function(s, r, o) {
		d = s.getModifiedRecords().length == 0
		saveBtn.setDisabled(d);
		rejectBtn.setDisabled(d);
	    }
	}
    });
  
    tvheadend.comet.on('dvbMux', function(m) {
	
	r = store.getById(m.id)
	if(typeof r === 'undefined') {
	    store.reload();
	    return;
	}

	for (key in m)
	    r.data[key] = m[key];

	store.afterEdit(r);
	store.fireEvent('updated', store, r, Ext.data.Record.COMMIT);
    });

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
		url: "dvb/muxes/" + adapterId,
		params: {
		    op:"delete",
		    entries:Ext.encode(selectedKeys)
		},
		failure:function(response,options) {
		    Ext.MessageBox.alert('Server Error','Unable to delete');
		},
		success:function(response,options) {
		    store.reload();
		}
	    })
	}
    }


     function copySelected() {

	 function doCopy() {
	     var selectedKeys = grid.selModel.selections.keys;
	     var target = panel.getForm().getValues('targetID').targetID;

	     Ext.Ajax.request({
		 url: "dvb/copymux/" + target,
		 params: {
		     entries:Ext.encode(selectedKeys)
		 },
		failure:function(response,options) {
		    Ext.MessageBox.alert('Server Error','Unable to copy');
		},
		 success: function() {
		     win.close();
		 }
	     });
	 }

	 targetStore = new Ext.data.JsonStore({
	     root:'entries',
	     id: 'identifier',
	     url:'dvb/adapter',
	     fields: ['identifier', 
		      'name'],
	     baseParams: {sibling: adapterId}
	 });

	 var panel = new Ext.FormPanel({
	     frame:true,
	     border:true,
	     bodyStyle:'padding:5px',
	     labelAlign: 'right',
	     labelWidth: 110,
	     defaultType: 'textfield',
	     items: [

		 new Ext.form.ComboBox({
		     store: targetStore,
		     fieldLabel: 'Target adapter',
		     name: 'targetadapter',
		     hiddenName: 'targetID',
		     editable: false,
		     allowBlank: false,
		     triggerAction: 'all',
		     mode: 'remote',
		     displayField:'name',
		     valueField:'identifier',
		     emptyText: 'Select target adapter...'
		 })
	     ],
	     buttons: [{
		 text: 'Copy',
		 handler: doCopy
             }]
	 });

	 win = new Ext.Window({
	     title: 'Copy multiplex configuration',
             layout: 'fit',
             width: 500,
             height: 120,
	     modal: true,
             plain: true,
             items: panel
	 });
	 win.show();
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
	    url: "dvb/muxes/" + adapterId,
	    params: {
		op:"update", 
		entries:Ext.encode(out)
	    },
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

    var delBtn = new Ext.Toolbar.Button({
	tooltip: 'Delete one or more selected muxes',
	iconCls:'remove',
	text: 'Delete selected...',
	handler: delSelected,
	disabled: true
    });

    var copyBtn = new Ext.Toolbar.Button({
	tooltip: 'Copy selected multiplexes to other adapter',
	iconCls:'clone',
	text: 'Copy to other adapter...',
	handler: copySelected,
	disabled: true
    });

    selModel.on('selectionchange', function(s) {
	delBtn.setDisabled(s.getCount() == 0);
	copyBtn.setDisabled(s.getCount() == 0);
    });

    var saveBtn = new Ext.Toolbar.Button({
	tooltip: 'Save any changes made (Changed cells have red borders).',
	iconCls:'save',
	text: "Save changes",
	handler: saveChanges,
	disabled: true
    });

    var rejectBtn = new Ext.Toolbar.Button({
	tooltip: 'Revert any changes made (Changed cells have red borders).',
	iconCls:'undo',
	text: "Revert changes",
	handler: function() {
	    store.rejectChanges();
	},
	disabled: true
    });

    var grid = new Ext.grid.EditorGridPanel({
	stripeRows: true,
	title: 'Multiplexes',
	plugins: [enabledColumn, qualityColumn],
	store: store,
	clicksToEdit: 2,
	cm: cm,
        viewConfig: {forceFit:true},
	selModel: selModel,
	tbar: [
	    delBtn, copyBtn, '-', saveBtn, rejectBtn, '-', {
		text: 'Add mux(es) manually...',
		iconCls:'add',
		handler: function() {
		    tvheadend.addMuxManually(adapterData, satConfStore)
		}
	    }
	]
    });

    return grid;
}


/**
 * DVB service grid
 */
tvheadend.dvb_services = function(adapterId) {

    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 45
    });

    var actions = new Ext.ux.grid.RowActions({
	header:'',
	dataIndex: 'actions',
	width: 45,
	actions: [
	    {
		iconCls:'info',
		qtip:'Detailed information about service',
		cb: function(grid, record, action, row, col) {
		    Ext.Ajax.request({
			url: "servicedetails/" + record.id,
			success:function(response, options) {
			    r = Ext.util.JSON.decode(response.responseText);
			    tvheadend.showTransportDetails(r);
			}
		    })
		}
	    }
	]
    });

    var cm = new Ext.grid.ColumnModel([
	enabledColumn,
	{
	    header: "Service name",
	    dataIndex: 'svcname',
	    width: 150
	},
	{
	    header: "Play",
	    dataIndex: 'id',
	    width: 50,
	    renderer: function(value, metadata, record, row, col, store) {
		url = 'stream/service/' + value
		return '<a href="'+url+'">Play</a>'
	    }
	},
	{
	    header: "Channel name",
	    dataIndex: 'channelname',
	    width: 150,
	    renderer: function(value, metadata, record, row, col, store) {
		return value ? value :
		    '<span class="tvh-grid-unset">Unmapped</span>';
	    },
	    editor: new fm.ComboBox({
		store: tvheadend.channels,
		allowBlank: true,
		typeAhead: true,
		minChars: 2,
		lazyRender: true,
		triggerAction: 'all',
		mode: 'local',
		displayField:'name'
	    })
	},
	{
		header: "DVB default charset",
		dataIndex: 'dvb_default_charset',
		width: 200,
		renderer: function(value, metadata, record, row, col, store) {
			return value ? value :
				'<span class="tvh-grid-unset">ISO6937</span>';
		},
		editor: new fm.ComboBox({
			mode: 'local',
			store: new Ext.data.SimpleStore({
				fields: ['key','value'],
				data: [
					['ISO6937','default'],
					['ISO6937','ISO6937'],
					['ISO8859-1','ISO8859-1'],
					['ISO8859-2','ISO8859-2'],
					['ISO8859-3','ISO8859-3'],
					['ISO8859-4','ISO8859-4'],
					['ISO8859-5','ISO8859-5'],
					['ISO8859-6','ISO8859-6'],
					['ISO8859-7','ISO8859-7'],
					['ISO8859-8','ISO8859-8'],
					['ISO8859-9','ISO8859-9'],
					['ISO8859-10','ISO8859-10'],
					['ISO8859-11','ISO8859-11'],
					['ISO8859-12','ISO8859-12'],
					['ISO8859-13','ISO8859-13'],
					['ISO8859-14','ISO8859-14'],
					['ISO8859-15','ISO8859-15']
       				]
			}),
			typeAhead: true,
			lazyRender: true,
			triggerAction: 'all',
			displayField:'value',
			valueField:'key'
		})
	},
	{
	    header: "Type",
	    dataIndex: 'type',
	    width: 50
	},
	{
	    header: "Provider",
	    dataIndex: 'provider',
	    width: 150
	},
	{
	    header: "Network",
	    dataIndex: 'network',
	    width: 100
	},
	{
	    header: "Multiplex",
	    dataIndex: 'mux',
	    width: 100
	},
	{
	    header: "Service ID",
	    dataIndex: 'sid',
	    width: 50,
	    hidden: true
	},
	{
	    header: "PMT PID",
	    dataIndex: 'pmt',
	    width: 50,
	    hidden: true
	},
	{
	    header: "PCR PID",
	    dataIndex: 'pcr',
	    width: 50,
	    hidden: true
	}, actions
    ]);

    cm.defaultSortable = true;

    var store = new Ext.data.JsonStore({
	root: 'entries',
	fields: Ext.data.Record.create([
	    'id', 'enabled', 'type', 'sid', 'pmt', 'pcr', 
	    'svcname', 'network', 'provider', 'mux', 'channelname', 'dvb_default_charset'
	]),
	url: "dvb/services/" + adapterId,
	autoLoad: true,
	id: 'id',
	baseParams: {op: "get"},
	listeners: {
	    'update': function(s, r, o) {
		d = s.getModifiedRecords().length == 0
		saveBtn.setDisabled(d);
		rejectBtn.setDisabled(d);
	    }
	}
    });

    var storeReloader = new Ext.util.DelayedTask(function() {
	store.reload()
    });
  
    tvheadend.comet.on('dvbService', function(m) {
	storeReloader.delay(500);
    });


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
    
 
    function saveChanges() {
	var mr = store.getModifiedRecords();
	var out = new Array();
	for (var x = 0; x < mr.length; x++) {
	    v = mr[x].getChanges();
	    out[x] = v;
	    out[x].id = mr[x].id;
	}

	Ext.Ajax.request({
	    url: "dvb/services/" + adapterId,
	    params: {
		op:"update", 
		entries:Ext.encode(out)
	    },
	    success:function(response,options) {
		store.commitChanges();
	    },
	    failure:function(response,options) {
		Ext.MessageBox.alert('Message',response.statusText);
	    }
	});
    }

    var saveBtn = new Ext.Toolbar.Button({
	tooltip: 'Save any changes made (Changed cells have red borders).',
	iconCls:'save',
	text: "Save changes",
	handler: saveChanges,
	disabled: true
    });

    var rejectBtn = new Ext.Toolbar.Button({
	tooltip: 'Revert any changes made (Changed cells have red borders).',
	iconCls:'undo',
	text: "Revert changes",
	handler: function() {
	    store.rejectChanges();
	},
	disabled: true
    });
       
    var selModel = new Ext.grid.RowSelectionModel({
	singleSelect:false
    });

    var grid = new Ext.grid.EditorGridPanel({
	stripeRows: true,
	title: 'Services',
	plugins: [enabledColumn, actions],
	store: store,
	clicksToEdit: 2,
	cm: cm,
        viewConfig: {forceFit:true},
	selModel: selModel,
	tbar: [saveBtn,  rejectBtn]
    });
    return grid;
}

/**
 *
 */
tvheadend.addMuxByLocation = function(adapterData, satConfStore) {
    
    var addBtn = new Ext.Button({
	text: 'Add DVB network',
	disabled: true,
	handler: function() {
	    var n = locationList.getSelectionModel().getSelectedNode();
	    Ext.Ajax.request({
		url: 'dvb/adapter/' + adapterData.identifier,
		params: {
		    network: n.attributes.id,
		    satconf: satConfCombo ? satConfCombo.getValue() : null,
		    op: 'addnetwork'
		}
	    });
	    win.close();
	}
    });

    if(satConfStore) {
	satConfCombo = new Ext.form.ComboBox({
	    store: satConfStore,
	    width: 480,
	    editable: false,
	    allowBlank: false,
	    triggerAction: 'all',
	    mode: 'remote',
	    displayField:'name',
	    valueField:'identifier',
	    emptyText: 'Select satellite configuration...'
	});
    } else {
	satConfCombo = false;
    }

    var locationList = new Ext.tree.TreePanel({
	title:'By location',
	autoScroll:true,
	rootVisible:false,
	loader: new Ext.tree.TreeLoader({
	    baseParams: {adapter: adapterData.identifier},
	    dataUrl:'dvbnetworks'
	}),
	
	root: new Ext.tree.AsyncTreeNode({
	    id:'root'
	}),
	
	bbar: [satConfCombo],

	buttons: [addBtn],
	buttonAlign: 'center'
    });
    

    locationList.on('click', function(n) {
	if(n.attributes.leaf) {
	    addBtn.enable();
	} else {
	    addBtn.disable();
	}
    });
	
    win = new Ext.Window({
	title: 'Add muxes on ' + adapterData.name,
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
	    items: locationList
        })
    });
    win.show();
}


/**
 * Add mux by manual configuration
 */
tvheadend.addMuxManually = function(adapterData, satConfStore) {
    
    var adId = adapterData.identifier;


    var items = [];

    switch(adapterData.deliverySystem) {
    case 'DVB-T':

	items.push(new Ext.form.NumberField({
	    fieldLabel: 'Frequency (kHz)',
	    name: 'frequency',
	    allowNegative: false,
	    allowBlank: false,
	    minValue: adapterData.freqMin,
	    maxValue: adapterData.freqMax
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Bandwidth',
	    name: 'bandwidth',
	    hiddenName: 'bandwidthID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/bandwidths/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Constellation',
	    name: 'constellation',
	    hiddenName: 'constellationID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/constellations/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Transmission mode',
	    name: 'tmode',
	    hiddenName: 'tmodeID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/transmissionmodes/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Guard interval',
	    name: 'guardinterval',
	    hiddenName: 'guardintervalID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/guardintervals/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Hierarchy',
	    name: 'hierarchy',
	    hiddenName: 'hierarchyID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/hierarchies/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'FEC Hi',
	    name: 'fechi',
	    hiddenName: 'fechiID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/fec/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'FEC Lo',
	    name: 'feclo',
	    hiddenName: 'fecloID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/fec/' + adId
	    })
	}));
	break;

    case 'DVB-C':
	items.push(new Ext.form.NumberField({
	    fieldLabel: 'Frequency (kHz)',
	    name: 'frequency',
	    allowNegative: false,
	    allowBlank: false,
	    minValue: adapterData.freqMin,
	    maxValue: adapterData.freqMax
	}));

	items.push(new Ext.form.NumberField({
	    fieldLabel: 'Symbolrate (baud)',
	    name: 'symbolrate',
	    allowNegative: false,
	    allowBlank: false,
	    minValue: adapterData.symrateMin,
	    maxValue: adapterData.symrateMax
	}));
 
	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Constellation',
	    name: 'constellation',
	    hiddenName: 'constellationID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/constellations/' + adId
	    })
	}));
	
	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'FEC',
	    name: 'fec',
	    hiddenName: 'fecID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/fec/' + adId
	    })
	}));
	break;

    case 'DVB-S':
	items.push(new Ext.form.NumberField({
	    fieldLabel: 'Frequency (kHz)',
	    name: 'frequency',
	    allowBlank: false,
	    allowNegative: false
	}));

	items.push(new Ext.form.NumberField({
	    fieldLabel: 'Symbolrate (baud)',
	    name: 'symbolrate',
	    allowNegative: false,
	    allowBlank: false,
	    minValue: adapterData.symrateMin,
	    maxValue: adapterData.symrateMax
	}));
 
	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'FEC',
	    name: 'fec',
	    hiddenName: 'fecID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/fec/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Delivery System',
	    name: 'delsys',
	    hiddenName: 'delsysID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/delsys/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Constellation',
	    name: 'constellation',
	    hiddenName: 'constellationID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/constellations/' + adId
	    })
	}));

	items.push(new Ext.form.ComboBox({
	    fieldLabel: 'Polarisation',
	    name: 'polarisation',
	    hiddenName: 'polarisationID',
	    editable: false,
	    allowBlank: false,
	    displayField: 'title',
	    valueField:'id',
	    mode:'remote',
	    triggerAction: 'all',
	    store: new Ext.data.JsonStore({
		root:'entries',
		fields: ['title', 'id'],
		url: 'dvb/feopts/polarisations/' + adId
	    })
	}));
    }
    
    if(satConfStore) {
	items.push(new Ext.form.ComboBox({
	    store: satConfStore,
	    fieldLabel: 'Satellite config',
	    name: 'satconf',
	    hiddenName: 'satconfID',
	    editable: false,
	    allowBlank: false,
	    triggerAction: 'all',
	    mode: 'remote',
	    displayField:'name',
	    valueField:'identifier'
	}));
    }

    function addMux() {
	panel.getForm().submit({
	    url:'dvb/addmux/' + adapterId, 
	    waitMsg:'Creating mux...'
	});
    }
 

    var panel = new Ext.FormPanel({
	frame:true,
	border:true,
	bodyStyle:'padding:5px',
	labelAlign: 'right',
	labelWidth: 110,
	defaultType: 'textfield',
	items: items,
	buttons: [{
	    text: 'Add',
	    handler: addMux
        }]
	
    });

    win = new Ext.Window({
	title: 'Add muxes on ' + adapterData.name,
        layout: 'fit',
        width: 500,
        height: 500,
        plain: true,
        items: panel
    });
    win.show();

}

/**
 * DVB adapter details
 */
tvheadend.dvb_adapter_general = function(adapterData, satConfStore) {

    adapterId = adapterData.identifier;

    var addMuxByLocationBtn = new Ext.Button({
	style:'margin:5px',
	iconCls:'add',
	text: 'Add DVB Network by location...',
	handler:function() {
	    tvheadend.addMuxByLocation(adapterData, satConfStore);
	}
    });

    var serviceScanBtn = new Ext.Button({
	style:'margin:5px',
	iconCls:'option',
	text: 'Map DVB services to channels...',
	disabled: adapterData.services == 0 || adapterData.initialMuxes,
	handler:function() {
	    Ext.Ajax.request({
		url:'dvb/adapter/' + adapterId, 
		params: {
		    op: 'serviceprobe'
		}
	    })
	}
    });


    /* Tool panel */

    var toolpanel = new Ext.Panel({
	layout: 'table',
	title: 'Tools',
	style:'margin:10px',
	bodyStyle:'padding:5px',
        columnWidth: .25,
	layoutConfig: {
            columns: 1
	},

	items: [
	    addMuxByLocationBtn,
	    serviceScanBtn
	]
    });

    /* Conf panel */

    var confreader = new Ext.data.JsonReader({
	root: 'dvbadapters'
    }, ['name', 'automux', 'idlescan', 'diseqcversion', 'qmon',
	'dumpmux', 'nitoid']);

    
    function saveConfForm () {
	confform.getForm().submit({
	    url:'dvb/adapter/' + adapterId, 
	    params:{'op':'save'},
	    waitMsg:'Saving Data...'
	});
    }

    var items = [
	{
	    fieldLabel: 'Adapter name',
	    name: 'name',
	    width: 250
	},
	new Ext.form.Checkbox({
	    fieldLabel: 'Autodetect muxes',
	    name: 'automux'
	}),
	new Ext.form.Checkbox({
	    fieldLabel: 'Idle scanning',
	    name: 'idlescan'
	}),
	new Ext.form.Checkbox({
	    fieldLabel: 'Monitor signal quality',
	    name: 'qmon'
	}),
	new Ext.form.Checkbox({
	    fieldLabel: 'Write full DVB MUX to disk',
	    name: 'dumpmux',
	    handler: function(s, v) {
		if(v)
		    Ext.MessageBox.alert('DVB Mux dump',
					 'Please note that keeping this ' +
					 'option enabled can consume a lot ' +
					 'of diskspace. You have been warned');
	    }
	}),
	{
	    fieldLabel: 'NIT-o Network ID',
	    name: 'nitoid',
	    width: 50
	}
    ];

    if(satConfStore) { 
	v = new Ext.form.ComboBox({
	    name: 'diseqcversion',
	    fieldLabel: 'DiSEqC version',
	    editable: false,
	    allowBlank: false,
	    mode: 'remote',
	    triggerAction: 'all',
	    store: ['DiSEqC 1.0 / 2.0', 'DiSEqC 1.1 / 2.1']
	});
	items.push(v);
    }
    
    var confform = new Ext.FormPanel({
	title:'Adapter configuration',
        columnWidth: .40,
	frame:true,
	border:true,
	disabled:true,
	style:'margin:10px',
	bodyStyle:'padding:5px',
	labelAlign: 'right',
	labelWidth: 160,
	waitMsgTarget: true,
	reader: confreader,
	defaultType: 'textfield',
	items: items,
	buttons: [{
	    text: 'Save',
	    handler: saveConfForm
        }]
    });
    
    confform.getForm().load({
	url:'dvb/adapter/' + adapterId, 
	params:{'op':'load'},
	success:function(form, action) {
	    confform.enable();
	}
    });

    /**
     * Information / capabilities panel 
     */
    
    var infoTemplate = new Ext.XTemplate(
	'<h2 style="font-size: 150%">Hardware</h2>' +
	    '<h3>Device path:</h3>{path}' +
	    '<h3>Device name:</h3>{devicename}' +
	    '<h3>Host connection:</h3>{hostconnection}' +
	    '<h3><tpl if="satConf != 0">Intermediate </tpl>Frequency range:</h3>{freqMin} kHz - {freqMax} kHz' +
	    ', in steps of {freqStep} kHz' +
	    '<tpl if="symrateMin != 0">' +
	    '<h3>Symbolrate range:</h3>' + 
	    '{symrateMin} Baud - {symrateMax} Baud</tpl>' +
	    '<h2 style="font-size: 150%">Status</h2>' +
	    '<h3>Currently tuned to:</h3>{currentMux}&nbsp' +
	    '<h3>Services:</h3>{services}' +
	    '<h3>Muxes:</h3>{muxes}' +
	    '<h3>Muxes awaiting initial scan:</h3>{initialMuxes}'
    );
   

    var infoPanel = new Ext.Panel({
	title:'Information and capabilities',
        columnWidth: .35,
	frame:true,
	border:true,
	style:'margin:10px',
	bodyStyle:'padding:5px',
	html: infoTemplate.applyTemplate(adapterData)
    });

    /**
     * Main adapter panel
     */
    var panel = new Ext.Panel({
	title: 'General',
	layout:'column',
	items: [toolpanel, confform, infoPanel]
    });


    /**
     * Subscribe and react on updates for this adapter
     */
    tvheadend.tvAdapterStore.on('update', function(s, r, o) {
	if(r.data.identifier != adapterId)
	    return;
	infoTemplate.overwrite(infoPanel.body, r.data);

	if(r.data.services > 0 && r.data.initialMuxes == 0)
	    serviceScanBtn.enable();
	else
	    serviceScanBtn.disable();
    });

    return panel;
}



/**
 *
 */
tvheadend.dvb_dummy = function(title)
{
    return new Ext.Panel({
	layout:'fit', 
	items:[{border: false}],
	title: title
    });
}

/**
 *
 */
tvheadend.dvb_satconf = function(adapterId, lnbStore)
{
    var fm = Ext.form;
 
    var cm = new Ext.grid.ColumnModel([
	{
	    header: "Name",
	    dataIndex: 'name',
	    width: 200,
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Switchport",
	    dataIndex: 'port',
	    editor: new fm.NumberField({
		minValue: 0,
		maxValue: 15
	    })
	},{
	    header: "LNB type",
	    dataIndex: 'lnb',
	    width: 200,
	    editor: new fm.ComboBox({
		store: lnbStore,
		editable: false,
		allowBlank: false,
		triggerAction: 'all',
		mode: 'remote',
		displayField:'identifier',
		valueField:'identifier',
		emptyText: 'Select LNB type...'
	    })
	},{
	    header: "Comment",
	    dataIndex: 'comment',
	    width: 400,
	    editor: new fm.TextField()
	}
    ]);

    var rec = Ext.data.Record.create([
	'name','port','comment','lnb'
    ]);

    return new tvheadend.tableEditor('Satellite config', 
				     'dvbsatconf/' + adapterId, cm, rec,
				     null, null, null);
}


/**
 *
 */
tvheadend.dvb_adapter = function(data)
{

    if(data.satConf) {
	var lnbStore = new Ext.data.JsonStore({
	    root:'entries',
	    autoload:true,
	    fields: ['identifier'],
	    url:'dvb/lnbtypes'
	});

	var satConfStore = new Ext.data.JsonStore({
	    root:'entries',
	    autoLoad: true,
	    id: 'identifier',
	    fields: ['identifier', 'name'],
	    url:'dvb/satconf/' + data.identifier
	});
    } else {
	satConfStore = false;
    }

    var items = [
	new tvheadend.dvb_adapter_general(data, satConfStore),
	new tvheadend.dvb_muxes(data, satConfStore),
	new tvheadend.dvb_services(data.identifier)
    ];

    if(data.satConf)
	items.push(new tvheadend.dvb_satconf(data.identifier, lnbStore));
    
    var panel = new Ext.TabPanel({
	border: false,
	activeTab:0, 
	autoScroll:true, 
	items: items
    });
    
    return panel;

}
