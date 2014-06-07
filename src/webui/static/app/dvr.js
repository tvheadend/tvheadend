tvheadend.weekdays = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [['1', 'Mon'], ['2', 'Tue'], ['3', 'Wed'], ['4', 'Thu'],
        ['5', 'Fri'], ['6', 'Sat'], ['7', 'Sun']]
});

//This should be loaded from tvheadend
tvheadend.dvrprio = new Ext.data.SimpleStore({
    fields: ['identifier', 'name'],
    id: 0,
    data: [['important', 'Important'], ['high', 'High'],
        ['normal', 'Normal'], ['low', 'Low'],
        ['unimportant', 'Unimportant']]
});

//For the container configuration
tvheadend.containers = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['name', 'description'],
    id: 'name',
    url: 'dvr_containers',
    baseParams: {
        op: 'list'
    }
});

//For the cache configuration
tvheadend.charsets = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['key', 'val'],
    id: 'key',
    url: 'api/intlconv/charsets',
    baseParams: {
        enum: 1
    }
});

//For the charset configuration
tvheadend.caches = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['index', 'description'],
    id: 'name',
    url: 'dvr_caches',
    baseParams: {
        op: 'list'
    }
});

/**
 * Configuration names
 */
tvheadend.configNames = new Ext.data.JsonStore({
    autoLoad: true,
    root: 'entries',
    fields: ['identifier', 'name'],
    id: 'identifier',
    url: 'confignames',
    baseParams: {
        op: 'list'
    }
});

tvheadend.configNames.setDefaultSort('name', 'ASC');

tvheadend.comet.on('dvrconfig', function(m) {
    if (m.reload != null)
        tvheadend.configNames.reload();
});

/**
 *
 */
tvheadend.dvrDetails = function(entry) {

    var content = '';
    var but;

    if (entry.chicon != null && entry.chicon.length > 0)
        content += '<img class="x-epg-chicon" src="'
                + entry.chicon + '">';

    content += '<div class="x-epg-title">' + entry.title + '</div>';
    content += '<div class="x-epg-desc">' + entry.description + '</div>';
    content += '<hr>';
    content += '<div class="x-epg-meta">Status: ' + entry.status + '</div>';

    if (entry.url != null && entry.filesize > 0) {
        content += '<div class="x-epg-meta">' + '<a href="' + entry.url
                + '" target="_blank">Download</a> '
                + parseInt(entry.filesize / 1000000) + ' MB</div>';

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

    switch (entry.schedstate) {
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
            params: {
                entryId: entry.id,
                op: 'cancelEntry'
            },
            success: function(response, options) {
                win.close();
            },
            failure: function(response, options) {
                Ext.MessageBox.alert('DVR', response.statusText);
            }
        });
    }

    function deleteEvent() {
        Ext.Ajax.request({
            url: 'dvr',
            params: {
                entryId: entry.id,
                op: 'deleteEntry'
            },
            success: function(response, options) {
                win.close();
            },
            failure: function(response, options) {
                Ext.MessageBox.alert('DVR', response.statusText);
            }
        });
    }

};

/**
 *
 */
tvheadend.dvrschedule = function(title, iconCls, dvrStore) {

    var actions = new Ext.ux.grid.RowActions({
        header: '',
        dataIndex: 'actions',
        width: 45,
        actions: [{
                iconIndex: 'schedstate'
            }]
    });

    function renderDate(value) {
        var dt = new Date(value);
        return dt.format('D j M H:i');
    }

    function renderDuration(value) {
        value = value / 60; /* Nevermind the seconds */

        if (value >= 60) {
            var min = parseInt(value % 60);
            var hours = parseInt(value / 60);

            if (min === 0) {
                return hours + ' hrs';
            }
            return hours + ' hrs, ' + min + ' min';
        }
        else {
            return parseInt(value) + ' min';
        }
    }

    function renderSize(value)
    {
        if (value == null)
            return '';
        return parseInt(value / 1000000) + ' MB';
    }

    function renderPri(value) {
        return tvheadend.dvrprio.getById(value).data.name;
    }

    var dvrCm = new Ext.grid.ColumnModel([actions, {
            width: 250,
            id: 'title',
            header: "Title",
            sortable: true,
            dataIndex: 'title'
        }, {
            width: 100,
            id: 'episode',
            header: "Episode",
            sortable: true,
            dataIndex: 'episode'
        }, {
            width: 100,
            id: 'pri',
            header: "Priority",
            sortable: true,
            dataIndex: 'pri',
            renderer: renderPri,
            hidden: iconCls !== 'clock'
        }, {
            width: 100,
            id: 'start',
            header: iconCls === 'clock' ? "Start" : "Date/Time",
            sortable: true,
            dataIndex: 'start',
            renderer: renderDate
        }, {
            width: 100,
            hidden: true,
            id: 'end',
            header: "End",
            sortable: true,
            dataIndex: 'end',
            renderer: renderDate
        }, {
            width: 100,
            id: 'duration',
            header: "Duration",
            sortable: true,
            dataIndex: 'duration',
            renderer: renderDuration
        }, {
            width: 100,
            id: 'filesize',
            header: "Filesize",
            sortable: true,
            dataIndex: 'filesize',
            renderer: renderSize,
            hidden: iconCls !== 'television'
        }, {
            width: 250,
            id: 'channel',
            header: "Channel",
            sortable: true,
            dataIndex: 'channel'
        }, {
            width: 200,
            id: 'creator',
            header: "Created by",
            sortable: true,
            hidden: true,
            dataIndex: 'creator'
        }, {
            width: 200,
            id: 'config_name',
            header: "DVR Configuration",
            sortable: true,
            renderer: function(value, metadata, record, row, col, store) {
                if (!value) {
                    return '<span class="tvh-grid-unset">(default)</span>';
                }
                else {
                    return value;
                }
            },
            dataIndex: 'config_name',
            hidden: iconCls !== 'clock'
        }, {
            width: 200,
            id: 'status',
            header: "Status",
            sortable: true,
            dataIndex: 'status',
            hidden: iconCls !== 'exclamation'
        }]);

    function addEntry() {

        function createRecording() {
            panel.getForm().submit({
                params: {
                    'op': 'createEntry'
                },
                url: 'dvr/addentry',
                waitMsg: 'Creating entry...',
                failure: function(response, options) {
                    Ext.MessageBox.alert('Server Error', 'Unable to create entry');
                },
                success: function() {
                    win.close();
                }
            });
        }

        var panel = new Ext.FormPanel({
            frame: true,
            border: true,
            bodyStyle: 'padding:5px',
            labelAlign: 'right',
            labelWidth: 110,
            defaultType: 'textfield',
            items: [new Ext.form.ComboBox({
                    fieldLabel: 'Channel',
                    name: 'channel',
                    hiddenName: 'channelid',
                    editable: false,
                    allowBlank: false,
                    displayField: 'val',
                    valueField: 'key',
                    mode: 'remote',
                    triggerAction: 'all',
                    store: tvheadend.channels
                }), new Ext.form.DateField({
                    allowBlank: false,
                    fieldLabel: 'Date',
                    name: 'date'
                }), new Ext.form.TimeField({
                    allowBlank: false,
                    fieldLabel: 'Start time',
                    name: 'starttime',
                    increment: 10,
                    format: 'H:i'
                }), new Ext.form.TimeField({
                    allowBlank: false,
                    fieldLabel: 'Stop time',
                    name: 'stoptime',
                    increment: 10,
                    format: 'H:i'
                }), new Ext.form.ComboBox({
                    store: tvheadend.dvrprio,
                    value: "normal",
                    triggerAction: 'all',
                    mode: 'local',
                    fieldLabel: 'Priority',
                    valueField: 'identifier',
                    displayField: 'name',
                    name: 'pri'
                }), {
                    allowBlank: false,
                    fieldLabel: 'Title',
                    name: 'title'
                }, new Ext.form.ComboBox({
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
                })],
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
        });
    }
    ;

    /* Create combobox to allow user to select page size for upcoming/completed/failed recordings */

    var itemPageCombo = new Ext.form.ComboBox({
        name : 'itemsperpage',
        width: 50,
        mode : 'local',
        store: new Ext.data.ArrayStore({
            fields: ['perpage','value'],
            data  : [['10',10],['20',20],['30',30],['40',40],['50',50],['75',75],['100',100],['All',9999999999]]
        }),
        value : '20',
        listWidth : 40,
        triggerAction : 'all',
        displayField : 'perpage',
        valueField : 'value',
        editable : true,
        forceSelection : true,
        listeners : {
            scope: this,
            'select' : function(combo, record) {
                bbar.pageSize = parseInt(record.get('value'), 10);
                bbar.doLoad(bbar.cursor);
            }
        }
    });

    /* Bottom toolbar to include default previous/goto-page/next and refresh buttons, also number-of-items combobox */
    
    var bbar = new Ext.PagingToolbar({
        store : dvrStore,
        displayInfo : true,
        items : ['-','Recordings per page: ',itemPageCombo],
        displayMsg : 'Programs {0} - {1} of {2}',
        emptyMsg : "No programs to display"
    });

    var panel = new Ext.grid.GridPanel({
        loadMask: true,
        stripeRows: true,
        disableSelection: true,
        title: title,
        iconCls: iconCls,
        store: dvrStore,
        cm: dvrCm,
        plugins: [actions],
        viewConfig: {
            forceFit: true
        },
        tbar: [{
                tooltip: 'Schedule a new recording session on the server.',
                iconCls: 'add',
                text: 'Add entry',
                handler: addEntry
            }, '->', {
                text: 'Help',
                handler: function() {
                    new tvheadend.help('Digital Video Recorder', 'dvrlog.html');
                }
            }],
        bbar: bbar
    });

    panel.on('rowclick', rowclicked);
    function rowclicked(grid, index) {
        new tvheadend.dvrDetails(grid.getStore().getAt(index).data);
    }
    return panel;
};

/**
 *
 */

/**
 *
 */
tvheadend.autoreceditor = function() {
    var fm = Ext.form;

    var cm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns:
                [
                    {
                        header: 'Enabled',
                        dataIndex: 'enabled',
                        width: 30,
                        xtype: 'checkcolumn'
                    },
                    {
                        header: "Title (Regexp)",
                        dataIndex: 'title',
                        editor: new fm.TextField({
                            allowBlank: true
                        })
                    },
                    {
                        header: "Channel",
                        dataIndex: 'channel',
                        editor: new Ext.form.ComboBox({
                            loadingText: 'Loading...',
                            displayField: 'val',
                            valueField: 'key',
                            store: tvheadend.channels,
                            mode: 'local',
                            editable: false,
                            triggerAction: 'all',
                            emptyText: 'Only include channel...'
                        }),
                        renderer: function(v, m, r) {
                            var i = tvheadend.channels.find('key', v);
                            if (i !== -1)
                                v = tvheadend.channels.getAt(i).get('val');
                            return v;
                        }
                    },
                    {
                        header: "SeriesLink",
                        dataIndex: 'serieslink',
                        renderer: function(v) {
                            return v ? 'yes' : 'no';
                        }
                    },
                    {
                        header: "Channel tag",
                        dataIndex: 'tag',
                        editor: new Ext.form.ComboBox({
                            displayField: 'name',
                            store: tvheadend.channelTags,
                            mode: 'local',
                            editable: false,
                            triggerAction: 'all',
                            emptyText: 'Only include tag...'
                        })
                    },
                    {
                        header: "Genre",
                        dataIndex: 'contenttype',
                        renderer: function(v) {
                            return tvheadend.contentGroupLookupName(v);
                        },
                        editor: new Ext.form.ComboBox({
                            valueField: 'code',
                            displayField: 'name',
                            store: tvheadend.ContentGroupStore,
                            mode: 'local',
                            editable: false,
                            triggerAction: 'all',
                            emptyText: 'Only include content...'
                        })
                    },
                    {
                        header: "Weekdays",
                        dataIndex: 'weekdays',
                        renderer: function(value, metadata, record, row, col, store) {
                            if (value.split)
                                value = value.split(',');
                            if (value.length === 7)
                                return 'All days';
                            if (value.length === 0 || value[0] === "")
                                return 'No days';
                            ret = [];
                            tags = value;
                            for (var i = 0; i < tags.length; i++) {
                                var tag = tvheadend.weekdays.getById(tags[i]);
                                if (typeof tag !== 'undefined')
                                    ret.push(tag.data.name);
                            }
                            return ret.join(', ');
                        },
                        editor: new Ext.ux.form.LovCombo({
                            store: tvheadend.weekdays,
                            mode: 'local',
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
                    }, {
                        header: "DVR Configuration",
                        dataIndex: 'config_name',
                        renderer: function(value, metadata, record, row, col, store) {
                            if (!value) {
                                return '<span class="tvh-grid-unset">(default)</span>';
                            }
                            else {
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
                    }, {
                        header: "Created by",
                        dataIndex: 'creator',
                        editor: new fm.TextField({
                            allowBlank: false
                        })
                    }, {
                        header: "Comment",
                        dataIndex: 'comment',
                        editor: new fm.TextField({
                            allowBlank: false
                        })
                    }]});

    return new tvheadend.tableEditor('Automatic Recorder', 'autorec', cm,
            tvheadend.autorecRecord, [], tvheadend.autorecStore,
            'autorec.html', 'wand');
};
/**
 *
 */
tvheadend.dvr = function() {

    function datastoreBuilder(url) {
        return new Ext.data.JsonStore({
            root: 'entries',
            totalProperty: 'totalCount',
            fields: [{
                    name: 'id'
                }, {
                    name: 'channel'
                }, {
                    name: 'title'
                }, {
                    name: 'episode'
                }, {
                    name: 'pri'
                }, {
                    name: 'description'
                }, {
                    name: 'chicon'
                }, {
                    name: 'start',
                    type: 'date',
                    dateFormat: 'U' /* unix time */
                }, {
                    name: 'end',
                    type: 'date',
                    dateFormat: 'U' /* unix time */
                }, {
                    name: 'config_name'
                }, {
                    name: 'status'
                }, {
                    name: 'schedstate'
                }, {
                    name: 'error'
                }, {
                    name: 'creator'
                }, {
                    name: 'duration'
                }, {
                    name: 'filesize'
                }, {
                    name: 'url'
                }],
            url: url,
            autoLoad: true,
            id: 'id'
        });
    }
    tvheadend.dvrStoreUpcoming = datastoreBuilder('dvrlist_upcoming');
    tvheadend.dvrStoreFinished = datastoreBuilder('dvrlist_finished');
    tvheadend.dvrStoreFailed = datastoreBuilder('dvrlist_failed');
    tvheadend.dvrStores = [tvheadend.dvrStoreUpcoming,
        tvheadend.dvrStoreFinished,
        tvheadend.dvrStoreFailed];


    function updateDvrStore(store, r, m) {
        r.data.status = m.status;
        r.data.schedstate = m.schedstate;

        store.afterEdit(r);
        store.fireEvent('updated', store, r,
                Ext.data.Record.COMMIT);
    }

    function reloadStores() {
        for (var i = 0; i < tvheadend.dvrStores.length; i++) {
            tvheadend.dvrStores[i].reload();
        }
    }

    tvheadend.comet.on('dvrdb', function(m) {

        if (m.reload != null) {
            reloadStores();
        }

        if (m.updateEntry != null) {
            for (var i = 0; i < tvheadend.dvrStores.length; i++) {
                var store = tvheadend.dvrStores[i];
                r = tvheadend.dvrStoreUpcoming.getById(m.id);
                if (typeof r !== 'undefined') {
                    updateDvrStore(store, r, m);
                    return;
                }
            }
            reloadStores();
        }
    });

    tvheadend.autorecRecord = Ext.data.Record.create(['enabled', 'title',
        'serieslink', 'channel', 'tag', 'creator', 'contenttype', 'comment',
        'weekdays', 'pri', 'approx_time', 'config_name']);

    tvheadend.autorecStore = new Ext.data.JsonStore({
        root: 'entries',
        fields: tvheadend.autorecRecord,
        url: "tablemgr",
        autoLoad: true,
        id: 'id',
        baseParams: {
            table: "autorec",
            op: "get"
        }
    });

    tvheadend.comet.on('autorec', function(m) {
        if (m.reload != null)
            tvheadend.autorecStore.reload();
    });

    var panel = new Ext.TabPanel({
        activeTab: 0,
        autoScroll: true,
        title: 'Digital Video Recorder',
        iconCls: 'drive',
        items: [
            new tvheadend.dvrschedule('Upcoming recordings', 'clock', tvheadend.dvrStoreUpcoming),
            new tvheadend.dvrschedule('Finished recordings', 'television', tvheadend.dvrStoreFinished),
            new tvheadend.dvrschedule('Failed recordings', 'exclamation', tvheadend.dvrStoreFailed),
            new tvheadend.autoreceditor
        ]
    });
    return panel;
};

/**
 * Configuration panel (located under configuration)
 */
tvheadend.dvrsettings = function() {

    var confreader = new Ext.data.JsonReader({
        root: 'dvrSettings'
    }, ['storage', 'filePermissions', 'dirPermissions', 'postproc', 'retention', 'dayDirs', 'channelDirs',
        'channelInTitle', 'container', 'cache', 'charset', 'dateInTitle', 'timeInTitle',
        'preExtraTime', 'postExtraTime', 'whitespaceInTitle', 'titleDirs',
        'episodeInTitle', 'cleanTitle', 'tagFiles', 'commSkip', 'subtitleInTitle',
        'episodeBeforeDate', 'rewritePAT', 'rewritePMT', 'episodeDuplicateDetection']);

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
        iconCls: 'remove',
        text: "Delete configuration",
        handler: deleteConfiguration,
        disabled: true
    });

    /* Config panel variables */

    /* DVR Behaviour */

    var recordingContainer = new Ext.form.ComboBox({
        store: tvheadend.containers,
        fieldLabel: 'Media container',
        triggerAction: 'all',
        displayField: 'description',
        valueField: 'name',
        editable: false,
        width: 350,
        hiddenName: 'container'
    });

    var cacheScheme = new Ext.form.ComboBox({
        store: tvheadend.caches,
        fieldLabel: 'Cache scheme',
        triggerAction: 'all',
        displayField: 'description',
        valueField: 'index',
        editable: false,
        width: 350,
        hiddenName: 'cache'
    });

    var logRetention = new Ext.form.NumberField({
        allowNegative: false,
        allowDecimals: false,
        minValue: 1,
        fieldLabel: 'DVR Log retention time (days)',
        name: 'retention'
    });

    var timeBefore = new Ext.form.NumberField({
        allowDecimals: false,
        fieldLabel: 'Extra time before recordings (minutes)',
        name: 'preExtraTime'
    });

    var timeAfter = new Ext.form.NumberField({
        allowDecimals: false,
        fieldLabel: 'Extra time after recordings (minutes)',
        name: 'postExtraTime'
    });

    var postProcessing = new Ext.form.TextField({
        width: 350,
        fieldLabel: 'Post-processor command',
        name: 'postproc'
    });

    /* Recording File Options */

    var recordingPath = new Ext.form.TextField({
        width: 350,
        fieldLabel: 'Recording system path',
        name: 'storage'
    });

    /* NB: recordingPermissions is defined as a TextField for validation purposes (leading zeros), but is ultimately a number */

    var recordingPermissions = new Ext.form.TextField({
        regex: /^[0][0-7]{3}$/,
        maskRe: /[0-7]/,
        width: 125,
        allowBlank: false,
        blankText: 'You must provide a value - use octal chmod notation, e.g. 0664',
        fieldLabel: 'File permissions (octal, e.g. 0664)',
        name: 'filePermissions'
    });

    var charset = new Ext.form.ComboBox({
        store: tvheadend.charsets,
        fieldLabel: 'Filename charset',
        triggerAction: 'all',
        displayField: 'val',
        valueField: 'key',
        editable: false,
        width: 200,
        hiddenName: 'charset'
    });

    /* TO DO - Add 'override user umask?' option, then trigger fchmod in mkmux.c, muxer_pass.c after file created */

    var PATrewrite = new Ext.form.Checkbox({
        fieldLabel: 'Rewrite PAT in passthrough mode',
        name: 'rewritePAT'
    });

    var PMTrewrite = new Ext.form.Checkbox({
        fieldLabel: 'Rewrite PMT in passthrough mode',
        name: 'rewritePMT'
    });

    var tagMetadata = new Ext.form.Checkbox({
        fieldLabel: 'Tag files with metadata',
        name: 'tagFiles'
    });

    var skipCommercials = new Ext.form.Checkbox({
        fieldLabel: 'Skip commercials',
        name: 'commSkip'
    });

    var episodeDuplicateDetection = new Ext.form.Checkbox({
        fieldLabel: 'Episode Duplicate Detect',
        name: 'episodeDuplicateDetection'
    });

    /* Subdirectories and filename handling */

    /* NB: directoryPermissions is defined as a TextField for validation purposes (leading zeros), but is ultimately a number */

    var directoryPermissions = new Ext.form.TextField({
        regex: /^[0][0-7]{3}$/,
        maskRe: /[0-7]/,
        width: 125,
        allowBlank: false,
        blankText: 'You must provide a value - use octal chmod notation, e.g. 0775',
        fieldLabel: 'Directory permissions (octal, e.g. 0775)',
        name: 'dirPermissions'
    });

    /* TO DO - Add 'override user umask?' option, then trigger fchmod in utils.c after directory created */

    var dirsPerDay = new Ext.form.Checkbox({
        fieldLabel: 'Make subdirectories per day',
        name: 'dayDirs'
    });

    var dirsPerChannel = new Ext.form.Checkbox({
        fieldLabel: 'Make subdirectories per channel',
        name: 'channelDirs'
    });

    var dirsPerTitle = new Ext.form.Checkbox({
        fieldLabel: 'Make subdirectories per title',
        name: 'titleDirs'
    });

    var incChannelInTitle = new Ext.form.Checkbox({
        fieldLabel: 'Include channel name in filename',
        name: 'channelInTitle'
    });

    var incDateInTitle = new Ext.form.Checkbox({
        fieldLabel: 'Include date in filename',
        name: 'dateInTitle'
    });

    var incTimeInTitle = new Ext.form.Checkbox({
        fieldLabel: 'Include time in filename',
        name: 'timeInTitle'
    });

    var incEpisodeInTitle = new Ext.form.Checkbox({
        fieldLabel: 'Include episode in filename',
        name: 'episodeInTitle'
    });

    var incSubtitleInTitle = new Ext.form.Checkbox({
        fieldLabel: 'Include subtitle in filename',
        name: 'subtitleInTitle'
    });

    var episodeFirst = new Ext.form.Checkbox({
        fieldLabel: 'Put episode in filename before date and time',
        name: 'episodeBeforeDate'
    });

    var stripUnsafeChars = new Ext.form.Checkbox({
        fieldLabel: 'Remove all unsafe characters from filename',
        name: 'cleanTitle'
    });

    var stripWhitespace = new Ext.form.Checkbox({
        fieldLabel: 'Replace whitespace in title with \'-\'',
        name: 'whitespaceInTitle'
    });

    /* Sub-Panel - DVR behaviour */

    var DVRBehaviour = new Ext.form.FieldSet({
        title: 'DVR Behaviour',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [recordingContainer, cacheScheme, logRetention, timeBefore, timeAfter, postProcessing]
    });

    /* Sub-Panel - File Output */

    var FileOutputPanel = new Ext.form.FieldSet({
        title: 'Recording File Options',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [recordingPath, recordingPermissions, charset, PATrewrite, PMTrewrite, tagMetadata, skipCommercials, episodeDuplicateDetection]
    });

    /* Sub-Panel - Directory operations */

    var DirHandlingPanel = new Ext.form.FieldSet({
        title: 'Subdirectory Options',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse: true,
        items: [directoryPermissions, dirsPerDay, dirsPerChannel, dirsPerTitle]
    });

    /* Sub-Panel - File operations - Break into two 4-item panels */

    var FileHandlingPanelA = new Ext.form.FieldSet({
        width: 350,
        border: false,
        autoHeight: true,
        items : [incChannelInTitle, incDateInTitle, incTimeInTitle, incEpisodeInTitle]
    });

    var FileHandlingPanelB = new Ext.form.FieldSet({
        width: 350,
        border: false,
        autoHeight: true,
        items : [incSubtitleInTitle, episodeFirst, stripUnsafeChars, stripWhitespace]
    });

    var FileHandlingPanel = new Ext.form.FieldSet({
        title: 'Filename Options',
        width: 700,
        autoHeight: true,
        collapsible: true,
        animCollapse : true,
        items : [{
            layout: 'column',
            border: false,
            items : [FileHandlingPanelA, FileHandlingPanelB] 
        }]
    });

    /* Main (form) panel */

    var confpanel = new Ext.FormPanel({
        title: 'Digital Video Recorder',
        iconCls: 'drive',
        border: false,
        bodyStyle: 'padding:15px',
        anchor: '100% 50%',
        labelAlign: 'right',
        labelWidth: 300,
        autoScroll: true,
        waitMsgTarget: true,
        reader: confreader,
        defaultType: 'textfield',
        layout: 'form',
        items: [DVRBehaviour, FileOutputPanel, DirHandlingPanel, FileHandlingPanel],
        tbar: [confcombo, {
                tooltip: 'Save changes made to dvr configuration below',
                iconCls: 'save',
                text: "Save configuration",
                handler: saveChanges
            }, delButton, '->', {
                text: 'Help',
                handler: function() {
                    new tvheadend.help('DVR configuration', 'config_dvr.html');
                }
            }]
    });

    function loadConfig() {
        confpanel.getForm().load({
            url: 'dvr',
            params: {
                'op': 'loadSettings',
                'config_name': confcombo.getValue()
            },
            success: function(form, action) {
                confpanel.enable();
            }
        });
    }

    confcombo.on('select', function() {
        if (confcombo.getValue() === '')
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
            url: 'dvr',
            params: {
                'op': 'saveSettings',
                'config_name': config_name
            },
            waitMsg: 'Saving Data...',
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
        if (confcombo.getValue() !== "") {
            Ext.MessageBox.confirm('Message',
                    'Do you really want to delete DVR configuration \''
                    + confcombo.getValue() + '\'?', deleteAction);
        }
    }

    function deleteAction(btn) {
        if (btn === 'yes') {
            confpanel.getForm().submit({
                url: 'dvr',
                params: {
                    'op': 'deleteSettings',
                    'config_name': confcombo.getValue()
                },
                waitMsg: 'Deleting Data...',
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
};
