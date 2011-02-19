
tvheadend.weekdays = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [
	['1', 'Mon'],
	['2', 'Tue'],
	['3', 'Wed'],
	['4', 'Thu'],
	['5', 'Fri'],
	['6', 'Sat'],
	['7', 'Sun']
    ]
});


// This should be loaded from tvheadend
tvheadend.dvrprio = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [
	['important',   'Important'],
	['high',        'High'],
	['normal',      'Normal'],
	['low',         'Low'],
	['unimportant', 'Unimportant']
    ]
});

/**
 * Configuration names
 */
tvheadend.configNames = new Ext.data.JsonStore({
    autoLoad:true,
    root:'entries',
    fields: ['identifier','name'],
    id: 'identifier',
    url:'confignames',
    baseParams: {
	op: 'list'
    }
});

tvheadend.configNames.setDefaultSort('name', 'ASC');

tvheadend.comet.on('dvrconfig', function(m) {
    if(m.reload != null)
        tvheadend.configNames.reload();
});


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
	    parseInt(entry.filesize/1000000) + ' MB<br>' + 
	    "<a href=\"javascript:tvheadend.VLC('playlist/dvrid/" + entry.id + "')\">Play</a>" +
	    '</div>';
    }

    var win = new Ext.Window({
	title: entry.title,
        layout: 'fit',
        width: 400,
        height: 300,
	constrainHeader: true,
	buttonAlign: 'center',
	html: content
    });

    switch(entry.schedstate) {
    case 'scheduled':
	win.addButton({
	    handler: cancelEvent,
	    text: "Remove from schedule"
	});
	break;

    case 'recording':
    case 'recordingError':
	win.addButton({
	    handler: cancelEvent, 
	    text: "Abort recording"
	});
	break;
	case 'completedError':
	case 'completed':
	win.addButton({
	    handler: deleteEvent, 
	    text: "Delete recording"
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

    function deleteEvent() {
	Ext.Ajax.request({
	    url: 'dvr',
	    params: {entryId: entry.id, op: 'deleteEntry'},

	    success:function(response, options) {
		win.close();v
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

    var actions = new Ext.ux.grid.RowActions({
	header:'',
	dataIndex: 'actions',
	width: 45,
	actions: [
	    {
		iconIndex:'schedstate'
	    }
	]
    });

    function renderDate(value){
	var dt = new Date(value);
	return dt.format('D j M H:i');
    } 

   function renderDuration(value){
       value = value / 60; /* Nevermind the seconds */
       
       if(value >= 60) {
	   var min = parseInt(value % 60);
	   var hours = parseInt(value / 60);

	   if(min == 0) {
	       return hours + ' hrs';
	   }
	   return hours + ' hrs, ' + min + ' min';
       } else {
	   return parseInt(value) + ' min';
       }
    } 

   function renderPri(value) {
       return tvheadend.dvrprio.getById(value).data.name;
   }

    var dvrCm = new Ext.grid.ColumnModel([
	actions,
	{
	    width: 250,
	    id:'title',
	    header: "Title",
	    dataIndex: 'title'
	},{
	    width: 100,
	    id:'episode',
	    header: "Episode",
	    dataIndex: 'episode'
	},{
	    width: 100,
	    id:'pri',
	    header: "Priority",
	    dataIndex: 'pri',
	    renderer: renderPri
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
            id:'config_name',
            header: "DVR Configuration",
            renderer: function(value, metadata, record, row, col, store) {
		if (!value) {
		    return '<span class="tvh-grid-unset">(default)</span>';
		} else {
		    return value;
		}
	    },
            dataIndex: 'config_name'
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
		new Ext.form.ComboBox({
		    store: tvheadend.dvrprio,
		    value: "normal",
		    triggerAction: 'all',
		    mode: 'local',
		    fieldLabel: 'Priority',
                    valueField: 'identifier',
                    displayField: 'name',
		    name: 'pri'
		}),
		{
		    allowBlank: false,
		    fieldLabel: 'Title',
		    name: 'title'
		},
		new Ext.form.ComboBox({
		    store: tvheadend.configNames,
		    triggerAction: 'all',
		    mode: 'local',
		    fieldLabel: 'DVR Configuration',
                    valueField: 'identifier',
                    displayField: 'name',
		    name: 'config_name',
                    emptyText: '(default)',
                    value: '',
                    editable: false
		})
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
		new Ext.form.ComboBox({
		    store: tvheadend.configNames,
		    triggerAction: 'all',
		    mode: 'local',
		    fieldLabel: 'DVR Configuration',
                    valueField: 'identifier',
                    displayField: 'name',
		    name: 'config_name',
                    emptyText: '(default)',
                    value: '',
                    editable: false
		})
    };


    var panel = new Ext.grid.GridPanel({
        loadMask: true,
	stripeRows: true,
	disableSelection: true,
	title: 'Recorder schedule',
	iconCls: 'clock',
	store: tvheadend.dvrStore,
	cm: dvrCm,
	plugins: [actions],
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
	}, {
	    header: "Weekdays",
	    dataIndex: 'weekdays',
	    renderer: function(value, metadata, record, row, col, store) {
		if (typeof value === 'undefined' || value.length < 1)
		    return 'No days';

		if (value == '1,2,3,4,5,6,7')
		    return 'All days';

		ret = [];
		tags = value.split(',');
		for (var i = 0; i < tags.length; i++) {
		    var tag = tvheadend.weekdays.getById(tags[i]);
		    if (typeof tag !== 'undefined')
			ret.push(tag.data.name);
		}
		return ret.join(', ');
	    },
	    editor: new Ext.ux.form.LovCombo({
		store: tvheadend.weekdays,
		mode:'local',
		valueField: 'identifier',
		displayField: 'name'
	    })
	}, {
        header: "Starting Around",
        dataIndex: 'approx_time',
        renderer: function(value, metadata, record, row, col, store) {
            if (typeof value === 'string')
                return value;

            if (value === 0)
                return '';

            var hours = Math.floor(value / 60);
            var mins = value % 60;
            var dt = new Date();
            dt.setHours(hours);
            dt.setMinutes(mins);
            return dt.format('H:i');
        },
        editor: new Ext.form.TimeField({
            allowBlank: true,
            increment: 10,
            format: 'H:i'
        })
	}, {
	    header: "Priority",
	    dataIndex: 'pri',
	    width: 100,
	    renderer: function(value, metadata, record, row, col, store) {
		return tvheadend.dvrprio.getById(value).data.name;
	    },
	    editor: new fm.ComboBox({
		store: tvheadend.dvrprio,
		triggerAction: 'all',
		mode: 'local',
                valueField: 'identifier',
                displayField: 'name'
	    })
        },{
	    header: "DVR Configuration",
	    dataIndex: 'config_name',
            renderer: function(value, metadata, record, row, col, store) {
		if (!value) {
		    return '<span class="tvh-grid-unset">(default)</span>';
		} else {
		    return value;
		}
	    },
            editor: new Ext.form.ComboBox({
                store: tvheadend.configNames,
                triggerAction: 'all',
                mode: 'local',
                valueField: 'identifier',
                displayField: 'name',
                name: 'config_name',
                emptyText: '(default)',
                editable: false
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
	    {name: 'episode'},
	    {name: 'pri'},
	    {name: 'description'},
	    {name: 'chicon'},
            {name: 'start', type: 'date', dateFormat: 'U' /* unix time */},
            {name: 'end', type: 'date', dateFormat: 'U' /* unix time */},
            {name: 'config_name'},
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

	if(m.updateEntry != null) {
	    r = tvheadend.dvrStore.getById(m.id)
	    if(typeof r === 'undefined') {
		tvheadend.dvrStore.reload();
		return;
	    }

	    r.data.status = m.status;
	    r.data.schedstate = m.schedstate;

	    tvheadend.dvrStore.afterEdit(r);
	    tvheadend.dvrStore.fireEvent('updated', 
					 tvheadend.dvrStore, r, 
					 Ext.data.Record.COMMIT);
	}
    });

    
    tvheadend.autorecRecord = Ext.data.Record.create([
	'enabled','title','channel','tag','creator','contentgrp','comment',
	'weekdays', 'pri', 'approx_time', 'config_name'
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
	'preExtraTime', 'postExtraTime', 'whitespaceInTitle', 
	'titleDirs', 'episodeInTitle', 'cleanTitle', 'tagFiles']);

    var confcombo = new Ext.form.ComboBox({
        store: tvheadend.configNames,
        triggerAction: 'all',
        mode: 'local',
        displayField: 'name',
        name: 'config_name',
        emptyText: '(default)',
        value: '',
        editable: true
    });

    var delButton = new Ext.Toolbar.Button({
        tooltip: 'Delete named configuration',
        iconCls:'remove',
        text: "Delete configuration",
        handler: deleteConfiguration,
        disabled: true
    });

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
	    fieldLabel: 'Make subdirectories per title',
	    name: 'titleDirs'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include channel name in filename',
	    name: 'channelInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Remove all unsafe characters from filename',
	    name: 'cleanTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include date in filename',
	    name: 'dateInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include time in filename',
	    name: 'timeInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Include episode in filename',
	    name: 'episodeInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Replace whitespace in title with \'-\'',
	    name: 'whitespaceInTitle'
	}), new Ext.form.Checkbox({
	    fieldLabel: 'Tag files with metadata',
	    name: 'tagFiles'
	}), {
	    width: 300,
	    fieldLabel: 'Post-processor command',
	    name: 'postproc'
        }],
	tbar: [confcombo, {
	    tooltip: 'Save changes made to dvr configuration below',
	    iconCls:'save',
	    text: "Save configuration",
	    handler: saveChanges
	}, delButton, '->', {
	    text: 'Help',
	    handler: function() {
		new tvheadend.help('DVR configuration', 
				   'config_dvr.html');
	    }
	}]
    });
    
    function loadConfig() {
	confpanel.getForm().load({
	    url:'dvr', 
	    params:{'op':'loadSettings','config_name':confcombo.getValue()},
	    success:function(form, action) {
		confpanel.enable();
	    }
	});
    }

    confcombo.on('select', function() {
        if (confcombo.getValue() == '')
            delButton.disable();
        else
            delButton.enable();
        loadConfig();
    });

    confpanel.on('render', function() {
        loadConfig();
    });


    function saveChanges() {
        var config_name = confcombo.getValue();
	confpanel.getForm().submit({
	    url:'dvr', 
	    params:{'op':'saveSettings','config_name':config_name},
	    waitMsg:'Saving Data...',
            success: function(form, action) {
                confcombo.setValue(config_name);
                confcombo.fireEvent('select');
            },
	    failure: function(form, action) {
		Ext.Msg.alert('Save failed', action.result.errormsg);
	    }
	});
    }

    function deleteConfiguration() {
        if (confcombo.getValue() != "") {
            Ext.MessageBox.confirm('Message',
                         'Do you really want to delete DVR configuration \'' + 
                                confcombo.getValue() + '\'?', 
                          deleteAction);
        }
    }
    
    function deleteAction(btn) {
      if (btn == 'yes') {
	confpanel.getForm().submit({
	    url:'dvr', 
	    params:{'op':'deleteSettings','config_name':confcombo.getValue()},
	    waitMsg:'Deleting Data...',
            success: function(form, action) {
                confcombo.setValue('');
                confcombo.fireEvent('select');
            },
	    failure: function(form, action) {
		Ext.Msg.alert('Delete failed', action.result.errormsg);
	    }
	});
      }
    }

    return confpanel;
}

