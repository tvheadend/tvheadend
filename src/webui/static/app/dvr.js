

/**
 *
 */
tvheadend.dvrDetails = function(entry) {

    var content = '';
    var but;

    if(entry.chicon != null && entry.chicon.length > 0)
	content += '<img class="x-epg-chicon" src="' + entry.chicon + '">';

    content += '<div class="x-epg-title">' + entry.title + '</div>';
    content += '<div class="x-epg-desc">' + entry.description + '</div>';
    content += '<hr>'
    content += '<div class="x-epg-meta">Status: ' + entry.status + '</div>';

    if(entry.url != null && entry.filesize > 0) {
	content += '<div class="x-epg-meta">' +
	    '<a href="' + entry.url + '" target="_blank">Download</a> '+
	    parseInt(entry.filesize/1000000) + ' MB</div>';
    }

    var win = new Ext.Window({
	title: entry.title,
	bodyStyle: 'margin: 5px',
        layout: 'fit',
        width: 400,
        height: 300,
	constrainHeader: true,
	buttonAlign: 'center',
	html: content
    });

    switch(entry.schedstate) {
    case 'sched':
	win.addButton({
	    handler: cancelEvent,
	    text: "Remove from schedule"
	});
	break;

    case 'rec':
	win.addButton({
	    handler: cancelEvent, 
	    text: "Abort recording"
	});
	break;
    }



    win.show();


    function cancelEvent() {
	Ext.Ajax.request({
	    url: 'dvr',
	    params: {entryId: entry.id, op: 'cancelEntry'},

	    success:function(response, options) {
		win.close();
	    },

	    failure:function(response, options) {
		Ext.MessageBox.alert('DVR', response.statusText);
	    }
	});
    }

}

/**
 *
 */
tvheadend.dvrschedule = function() {

    function renderDate(value){
	var dt = new Date(value);
	return dt.format('l H:i');
    } 

   function renderDuration(value){
       value = value / 60; /* Nevermind the seconds */
       
       if(value >= 60) {
	   var min = value % 60;
	   var hours = parseInt(value / 60);

	   if(min == 0) {
	       return hours + ' hrs';
	   }
	   return hours + ' hrs, ' + min + ' min';
       } else {
	   return value + ' min';
       }
    } 

    var dvrCm = new Ext.grid.ColumnModel([
	{
	    width: 250,
	    id:'title',
	    header: "Title",
	    dataIndex: 'title'
	},{
	    width: 100,
	    id:'start',
	    header: "Start",
	    dataIndex: 'start',
	    renderer: renderDate
	},{
	    width: 100,
	    hidden:true,
	    id:'end',
	    header: "End",
	    dataIndex: 'end',
	    renderer: renderDate
	},{
	    width: 100,
	    id:'duration',
	    header: "Duration",
	    dataIndex: 'duration',
	    renderer: renderDuration
	},{
	    width: 250,
	    id:'channel',
	    header: "Channel",
	    dataIndex: 'channel'
	},{
	    width: 200,
	    id:'creator',
	    header: "Created by",
	    hidden:true,
	    dataIndex: 'creator'
	},{
	    width: 200,
	    id:'status',
	    header: "Status",
	    dataIndex: 'status'
	}
    ]);

    function addEntry() {

	function createRecording() {
	    panel.getForm().submit({
		params:{'op':'createEntry'},
		url:'dvr/addentry',
		waitMsg:'Creating entry...',
		failure:function(response, options) {
		    Ext.MessageBox.alert('Server Error',
					 'Unable to create entry');
		},
		success: function() {
		    win.close();
		}
	    });
	}
 
	var panel = new Ext.FormPanel({
	    frame:true,
	    border:true,
	    bodyStyle:'padding:5px',
	    labelAlign: 'right',
	    labelWidth: 110,
	    defaultType: 'textfield',
	    items: [
		new Ext.form.ComboBox({
		    fieldLabel: 'Channel',
		    name: 'channel',
		    hiddenName: 'channelid',
		    editable: false,
		    allowBlank: false,
		    displayField: 'name',
		    valueField:'chid',
		    mode:'remote',
		    triggerAction: 'all',
		    store: tvheadend.channels
		}),
		new Ext.form.DateField({
		    allowBlank: false,
		    fieldLabel: 'Date',
		    name: 'date'
		}),
		new Ext.form.TimeField({
		    allowBlank: false,
		    fieldLabel: 'Start time',
		    name: 'starttime',
		    increment: 10,
		    format: 'H:i'
		}),
		new Ext.form.TimeField({
		    allowBlank: false,
		    fieldLabel: 'Stop time',
		    name: 'stoptime',
		    increment: 10,
		    format: 'H:i'
		}),
		{
		    allowBlank: false,
		    fieldLabel: 'Title',
		    name: 'title'
		}
	    ],
	    buttons: [{
		text: 'Create',
		handler: createRecording
            }]
	    
	});
	
	win = new Ext.Window({
	    title: 'Add single recording',
            layout: 'fit',
            width: 500,
            height: 300,
            plain: true,
            items: panel
	});
	win.show();	
    };


    var panel = new Ext.grid.GridPanel({
        loadMask: true,
	stripeRows: true,
	disableSelection: true,
	title: 'Recorder schedule',
	iconCls: 'clock',
	store: tvheadend.dvrStore,
	cm: dvrCm,
        viewConfig: {forceFit:true},
	tbar: [
	    {
		tooltip: 'Schedule a new recording session on the server.',
		iconCls:'add',
		text: 'Add entry',
		handler: addEntry
	    },'->',{
		text: 'Help',
		handler: function() {
		    new tvheadend.help('Digital Video Recorder',
				       'dvrlog.html');
		}
	    }
	],
        bbar: new Ext.PagingToolbar({
            store: tvheadend.dvrStore,
            pageSize: 20,
            displayInfo: true,
            displayMsg: 'Programs {0} - {1} of {2}',
            emptyMsg: "No programs to display"
        })

    });

    
    panel.on('rowclick', rowclicked);
    function rowclicked(grid, index) {
	new tvheadend.dvrDetails(grid.getStore().getAt(index).data);
    }
    return panel;
}

/**
 *
 */


/**
 *
 */
tvheadend.autoreceditor = function() {
    var fm = Ext.form;
    
    var enabledColumn = new Ext.grid.CheckColumn({
       header: "Enabled",
       dataIndex: 'enabled',
       width: 30
    });

    var cm = new Ext.grid.ColumnModel([
	enabledColumn,
	{
	    header: "Title (Regexp)",
	    dataIndex: 'title',
	    editor: new fm.TextField({allowBlank: true})
	},{
	    header: "Channel",
	    dataIndex: 'channel',
	    editor: new Ext.form.ComboBox({
		loadingText: 'Loading...',
		displayField:'name',
		store: tvheadend.channels,
		mode: 'local',
		editable: false,
		triggerAction: 'all',
		emptyText: 'Only include channel...'
	    })
	},{
	    header: "Channel tag",
	    dataIndex: 'tag',
	    editor: new Ext.form.ComboBox({
		displayField:'name',
		store: tvheadend.channelTags,
		mode: 'local',
		editable: false,
		triggerAction: 'all',
		emptyText: 'Only include tag...'
	    })
	},{
	    header: "Content Group",
	    dataIndex: 'contentgrp',
	    editor: new Ext.form.ComboBox({
		displayField:'name',
		store: tvheadend.ContentGroupStore,
		mode: 'local',
		editable: false,
		triggerAction: 'all',
		emptyText: 'Only include content...'
	    })

	},{
	    header: "Created by",
	    dataIndex: 'creator',
	    editor: new fm.TextField({allowBlank: false})
	},{
	    header: "Comment",
	    dataIndex: 'comment',
	    editor: new fm.TextField({allowBlank: false})
	}
    ]);


    return new tvheadend.tableEditor('Automatic Recorder',
				     'autorec', cm, tvheadend.autorecRecord,
				     [enabledColumn], tvheadend.autorecStore,
				     'autorec.html', 'wand');
}
/**
 *
 */
tvheadend.dvr = function() {


    tvheadend.dvrStore = new Ext.data.JsonStore({
	root: 'entries',
	totalProperty: 'totalCount',
	fields: [
	    {name: 'id'},
	    {name: 'channel'},
	    {name: 'title'},
	    {name: 'description'},
	    {name: 'chicon'},
            {name: 'start', type: 'date', dateFormat: 'U' /* unix time */},
            {name: 'end', type: 'date', dateFormat: 'U' /* unix time */},
	    {name: 'status'},
	    {name: 'schedstate'},
	    {name: 'creator'},
            {name: 'duration'},
            {name: 'filesize'},
            {name: 'url'}
	],
	url: 'dvrlist',
	autoLoad: true,
	id: 'id',
	remoteSort: true
    });
    
    tvheadend.comet.on('dvrdb', function(m) {
	if(m.reload != null)
            tvheadend.dvrStore.reload();
    });

    
    tvheadend.autorecRecord = Ext.data.Record.create([
	'enabled','title','channel','tag','creator','contentgrp','comment'
    ]);
    

    tvheadend.autorecStore = new Ext.data.JsonStore({
	root: 'entries',
	fields: tvheadend.autorecRecord,
	url: "tablemgr",
	autoLoad: true,
	id: 'id',
	baseParams: {table: "autorec", op: "get"}
    });
    
    tvheadend.comet.on('autorec', function(m) {
	if(m.reload != null)
            tvheadend.autorecStore.reload();
    });


    var panel = new Ext.TabPanel({
	activeTab:0, 
	autoScroll:true, 
	title: 'Digital Video Recorder', 
	iconCls: 'drive',
	items: [new tvheadend.dvrschedule,
		new tvheadend.autoreceditor
	       ]
    });
    return panel;
}



/**
 * Configuration panel (located under configuration)
 */
tvheadend.dvrsettings = function() {

    var confreader = new Ext.data.JsonReader({
	root: 'dvrSettings'
    }, ['storage','postproc','retention','dayDirs',
	'channelDirs','channelInTitle',
	'dateInTitle','timeInTitle',
	'preExtraTime', 'postExtraTime']);

    var confpanel = new Ext.FormPanel({
	title:'Digital Video Recorder',
	iconCls: 'drive',
	border:false,
	bodyStyle:'padding:15px',
	anchor: '100% 50%',
	labelAlign: 'right',
	labelWidth: 250,
	waitMsgTarget: true,
	reader: confreader,
	defaultType: 'textfield',
	layout: 'form',
	items: [{
	    width: 300,
	    fieldLabel: 'Recording system path',
	    name: 'storage'
	}, new Ext.form.NumberField({
	    allowNegative: false,
	    allowDecimals: false,
	    minValue: 1,
	    fieldLabel: 'DVR Log retention time (days)',
	    name: 'retention'
	}), new Ext.form.NumberField({
	    allowDecimals: false,
	    fieldLabel: 'Extra time before recordings (minutes)',
	    name: 'preExtraTime'
	}), new Ext.form.NumberField({
	    allowDecimals: false,
	    fieldLabel: 'Extra time after recordings (minutes)',
	    name: 'postExtraTime'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Make subdirectories per day',
	    name: 'dayDirs'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Make subdirectories per channel',
	    name: 'channelDirs'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include channel name in title',
	    name: 'channelInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include date in title',
	    name: 'dateInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include time in title',
	    name: 'timeInTitle'
	}), {
	    width: 300,
	    fieldLabel: 'Post-processor command',
	    name: 'postproc'
        }],
	tbar: [{
	    tooltip: 'Save changes made to channel configuration below',
	    iconCls:'save',
	    text: "Save configuration",
	    handler: saveChanges
	}, '->', {
	    text: 'Help',
	    handler: function() {
		new tvheadend.help('DVR configuration', 
				   'config_dvr.html');
	    }
	}]
	
    });

    confpanel.on('render', function() {
	confpanel.getForm().load({
	    url:'dvr', 
	    params:{'op':'loadSettings'},
	    success:function(form, action) {
		confpanel.enable();
	    }
	});
    });


    function saveChanges() {
	confpanel.getForm().submit({
	    url:'dvr', 
	    params:{'op':'saveSettings'},
	    waitMsg:'Saving Data...',
	    failure: function(form, action) {
		Ext.Msg.alert('Save failed', action.result.errormsg);
	    }
	});
    }

    return confpanel;
}

