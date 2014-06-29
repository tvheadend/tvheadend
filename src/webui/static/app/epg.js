tvheadend.brands = new Ext.data.JsonStore({
    root: 'entries',
    fields: ['uri', 'title'],
    autoLoad: true,
    url: 'epgobject',
    baseParams: {
        op: 'brandList'
    }
});

insertContentGroupClearOption = function( scope, records, options ){
	var placeholder = scope.getAt(1); //create a 'template' copy of an existing record
	placeholder.set('code',-1);
	placeholder.set('name',"(Clear filter)");
	scope.insert(0, placeholder);
};

//WIBNI: might want this store to periodically update

tvheadend.ContentGroupStore = new Ext.data.JsonStore({
    root: 'entries',
    fields: ['name', 'code'],
    autoLoad: true,
    url: 'ecglist',
    listeners: {
        'load': insertContentGroupClearOption
    }
});

tvheadend.contentGroupLookupName = function(code) {
    ret = "";
    tvheadend.ContentGroupStore.each(function(r) {
        if (r.data.code === code)
            ret = r.data.name;
        else if (ret === "" && r.data.code === (code & 0xF0))
            ret = r.data.name;
    });
    return ret;
};

tvheadend.ContentGroupStore.setDefaultSort('code', 'ASC');

tvheadend.channelLookupName = function(key) {
    channelString = "";
          
    var index = tvheadend.channels.find('key', key);
    
    if (index !== -1)
        var channelString = tvheadend.channels.getAt(index).get('val');
    
    return channelString;
};  

// Store for duration filters - EPG, autorec dialog and autorec rules in the DVR grid
// NB: 'no max' is defined as 9999999s, or about 3 months...

tvheadend.DurationStore = new Ext.data.SimpleStore({
	storeId: 'durationnames',
	idIndex: 0,
    fields: ['identifier','label','minvalue','maxvalue'],
    data: [['-1', '(Clear filter)',"",""],
           ['1','00:00:01 - 00:15:00',1, 900],
           ['2','00:15:01 - 00:30:00', 901, 1800],
           ['3','00:30:01 - 01:30:00', 1801, 5400],
           ['4','01:30:01 - 03:00:00', 5401, 10800],
           ['5','03:00:01 - No maximum', 10801, 9999999]]
});

// Function to convert numeric duration to corresponding label string
// Note: triggered by minimum duration only. This would fail if ranges 
// had the same minimum (e.g. 15-30 mins and 15-60 minutes) (which we don't have). 

tvheadend.durationLookupRange = function(value) {
    durationString = "";
    
    var index = tvheadend.DurationStore.find('minvalue', value); 

    if (index !== -1)
        var durationString = tvheadend.DurationStore.getById(index).data.label;
    
    return durationString;
};  

tvheadend.epgDetails = function(event) {

    var content = '';

    if (event.chicon != null && event.chicon.length > 0)
        content += '<img class="x-epg-chicon" src="' + event.chicon + '">';

    content += '<div class="x-epg-title">' + event.title;
    if (event.subtitle)
        content += "&nbsp;:&nbsp;" + event.subtitle;
    content += '</div>';
    content += '<div class="x-epg-desc">' + event.episode + '</div>';
    content += '<div class="x-epg-desc">' + event.description + '</div>';
    content += '<div class="x-epg-meta">' + event.starrating + '</div>';
    content += '<div class="x-epg-meta">' + event.agerating + '</div>';
    content += '<div class="x-epg-meta">' + tvheadend.contentGroupLookupName(event.contenttype) + '</div>';

    if (event.ext_desc != null)
        content += '<div class="x-epg-meta">' + event.ext_desc + '</div>';

    if (event.ext_item != null)
        content += '<div class="x-epg-meta">' + event.ext_item + '</div>';

    if (event.ext_text != null)
        content += '<div class="x-epg-meta">' + event.ext_text + '</div>';

    content += '<div class="x-epg-meta"><a target="_blank" href="http://akas.imdb.com/find?q=' + event.title + '">Search IMDB</a></div>';
    content += '<div id="related"></div>';
    content += '<div id="altbcast"></div>';
    
    now = new Date();
    if (event.start < now && event.end > now) {
        var title = event.title;
        if (event.episode)
          title += ' / ' + event.episode;
        content += '<div class="x-epg-meta"><a href="play/stream/channelid/' + event.channelid +
                   '?title=' + encodeURIComponent(title) + '">Play</a></div>';
    }

    var confcombo = new Ext.form.ComboBox({
        store: tvheadend.configNames,
        triggerAction: 'all',
        mode: 'local',
        valueField: 'identifier',
        displayField: 'name',
        name: 'config_name',
        emptyText: '(default)',
        value: '',
        editable: false
    });

    var win = new Ext.Window({
        title: event.title,
        layout: 'fit',
        width: 500,
        height: 300,
        constrainHeader: true,
        buttons: [confcombo, new Ext.Button({
                handler: recordEvent,
                text: "Record program"
            }), new Ext.Button({
                handler: recordSeries,
                text: event.serieslink ? "Record series" : "Autorec"
            })],
        buttonAlign: 'center',
        html: content
    });
    win.show();

    function recordEvent() {
        record('recordEvent');
    }

    function recordSeries() {
        record('recordSeries');
    }

    function record(op) {
        Ext.Ajax.request({
            url: 'dvr',
            params: {
                eventId: event.id,
                op: op,
                config_name: confcombo.getValue()
            },
            success: function(response, options) {
                win.close();
            },
            failure: function(response, options) {
                Ext.MessageBox.alert('DVR', response.statusText);
            }
        });
    }

    function showAlternatives(s) {
        var e = Ext.get('altbcast');
        html = '';
        if (s.getTotalCount() > 0) {
            html += '<div class="x-epg-subtitle">Alternative Broadcasts</div>';
            for (i = 0; i < s.getTotalCount(); i++) {
                var ab = s.getAt(i).data;
                var dt = Date.parseDate(ab.start, 'U');
                html += '<div class="x-epg-desc">' + dt.format('l H:i')
                        + '&nbsp;&nbsp;&nbsp;' + ab.channel + '</div>';
            }
        }
        e.dom.innerHTML = html;
    }
    function showRelated(s) {
        var e = Ext.get('related');
        html = '';
        if (s.getTotalCount() > 0) {
            html += '<div class="x-epg-subtitle">Related Episodes</div>';
            for (i = 0; i < s.getTotalCount(); i++) {
                var ee = s.getAt(i).data;
                html += '<div class="x-epg-desc">';
                if (ee.episode)
                    html += ee.episode + '&nbsp;&nbsp;&nbsp;';
                html += ee.title;
                if (ee.subtitle)
                    html += ' : ' + ee.subtitle;
                html += '</div>';
            }
        }
        e.dom.innerHTML = html;
    }

    var ab = new Ext.data.JsonStore({
        root: 'entries',
        url: 'epgrelated',
        autoLoad: false,
        id: 'id',
        baseParams: {
            op: 'get',
            id: event.id,
            type: 'alternative'
        },
        fields: Ext.data.Record.create(['id', 'channel', 'start']),
        listeners: {
            'datachanged': showAlternatives
        }
    });
    var re = new Ext.data.JsonStore({
        root: 'entries',
        url: 'epgrelated',
        autoLoad: false,
        id: 'uri',
        baseParams: {
            op: 'get',
            id: event.id,
            type: 'related'
        },
        fields: Ext.data.Record
                .create(['uri', 'title', 'subtitle', 'episode']),
        listeners: {
            'datachanged': showRelated
        }
    });
};

tvheadend.epg = function() {
    var xg = Ext.grid;

    var actions = new Ext.ux.grid.RowActions({
        header: '',
        width: 20,
        dataIndex: 'actions',
        actions: [{
                iconIndex: 'schedstate'
            }]
    });

    var epgStore = new Ext.ux.grid.livegrid.Store({
        autoLoad: true,
        url: 'epg',
        bufferSize: 300,
        reader: new Ext.ux.grid.livegrid.JsonReader({
            root: 'entries',
            totalProperty: 'totalCount',
            id: 'id'
        }, [{
                name: 'id'
            }, {
                name: 'channel'
            }, {
                name: 'channelid'
            }, {
                name: 'title'
            }, {
                name: 'subtitle'
            }, {
                name: 'episode'
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
                name: 'duration'
            }, {
                name: 'starrating'
            }, {
                name: 'agerating'
            }, {
                name: 'contenttype'
            }, {
                name: 'schedstate'
            }, {
                name: 'serieslink'
            }])
    });

    function setMetaAttr(meta, record) {
        var now = new Date;
        var start = record.get('start');

        if (now.getTime() > start.getTime()) {
            meta.attr = 'style="font-weight:bold;"';
        }
    }

    function renderDate(value, meta, record, rowIndex, colIndex, store) {
        setMetaAttr(meta, record);

        var dt = new Date(value);
        return dt.format('D, M d, H:i');
    }

    function renderDuration(value, meta, record, rowIndex, colIndex, store) {
        setMetaAttr(meta, record);

        value = Math.floor(value / 60);

        if (value >= 60) {
            var min = value % 60;
            var hours = Math.floor(value / 60);

            if (min === 0) {
                return hours + ' hrs';
            }
            return hours + ' hrs, ' + min + ' min';
        }
        else {
            return value + ' min';
        }
    }

    function renderText(value, meta, record, rowIndex, colIndex, store) {
        setMetaAttr(meta, record);

        return value;
    }

    function renderInt(value, meta, record, rowIndex, colIndex, store) {
        setMetaAttr(meta, record);

        return '' + value;
    }

    var epgCm = new Ext.grid.ColumnModel([actions, {
            width: 250,
            id: 'title',
            header: "Title",
            dataIndex: 'title',
            renderer: renderText
        }, {
            width: 250,
            id: 'subtitle',
            header: "SubTitle",
            dataIndex: 'subtitle',
            renderer: renderText
        }, {
            width: 100,
            id: 'episode',
            header: "Episode",
            dataIndex: 'episode',
            renderer: renderText
        }, {
            width: 100,
            id: 'start',
            header: "Start",
            dataIndex: 'start',
            renderer: renderDate
        }, {
            width: 100,
            hidden: true,
            id: 'end',
            header: "End",
            dataIndex: 'end',
            renderer: renderDate
        }, {
            width: 100,
            id: 'duration',
            header: "Duration",
            dataIndex: 'duration',
            renderer: renderDuration
        }, {
            width: 250,
            id: 'channel',
            header: "Channel",
            dataIndex: 'channel',
            renderer: renderText
        }, {
            width: 50,
            id: 'starrating',
            header: "Stars",
            dataIndex: 'starrating',
            renderer: renderInt
        }, {
            width: 50,
            id: 'agerating',
            header: "Age",
            dataIndex: 'agerating',
            renderer: renderInt
        }, {
            width: 250,
            id: 'contenttype',
            header: "Content Type",
            dataIndex: 'contenttype',
            renderer: function(v) {
                return tvheadend.contentGroupLookupName(v);
            }
        }]);

    // Title search box

    var epgFilterTitle = new Ext.form.TextField({
        emptyText: 'Search title...',
        width: 200
    });

    // Channels, uses global store

    var epgFilterChannels = new Ext.form.ComboBox({
        loadingText: 'Loading...',
        width: 200,
        displayField: 'val',
        store: tvheadend.channels,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        forceSelection: true,
        typeAhead: true,
        emptyText: 'Filter channel...',
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearChannelFilter();
                    epgStore.reload();
                }
            }
        }
    });

    // Tags, uses global store

    var epgFilterChannelTags = new Ext.form.ComboBox({
        width: 200,
        displayField: 'name',
        store: tvheadend.channelTags,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        forceSelection: true,
        typeAhead: true,
        emptyText: 'Filter tag...',
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearChannelTagsFilter();
                    epgStore.reload();
                }
            }
        }

    });

    // Content groups

    var epgFilterContentGroup = new Ext.form.ComboBox({
        loadingText: 'Loading...',
        width: 200,
        displayField: 'name',
        store: tvheadend.ContentGroupStore,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        forceSelection: true,
        typeAhead: true,
        emptyText: 'Filter content type...',
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearContentGroupFilter();
                    epgStore.reload();
                }
            }
        }
    });

    var epgFilterDuration = new Ext.form.ComboBox({
        loadingText: 'Loading...',
        width: 150,
        displayField: 'label',
        store: tvheadend.DurationStore,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        forceSelection: true,
        typeAhead: true,
        emptyText: 'Filter duration...',
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearDurationFilter();
                    epgStore.reload();
                }
            }
        }

    });

/* 
 * Clear filter functions
 */

    clearTitleFilter = function() {
        delete epgStore.baseParams.title;
        epgFilterTitle.setValue("");
    };

    clearChannelFilter = function() {
        delete epgStore.baseParams.channel;
        epgFilterChannels.setValue("");
    };

    clearChannelTagsFilter = function() {
        delete epgStore.baseParams.tag;
        epgFilterChannelTags.setValue("");
    };

    clearContentGroupFilter = function() {
		delete epgStore.baseParams.contenttype;
        epgFilterContentGroup.setValue("");
    };

    clearDurationFilter = function() {
   	    delete epgStore.baseParams.minduration;
        delete epgStore.baseParams.maxduration;
        epgFilterDuration.setValue("");
    };
    
    function epgQueryClear() {
        clearTitleFilter();
        clearChannelFilter();
        clearChannelTagsFilter();
        clearDurationFilter();
        clearContentGroupFilter();
        epgStore.reload();
    };

/*
 * Filter selection event handlers
 */
 
    epgFilterChannels.on('select', function(c, r) {
        if (r.data.key == -1) 
            clearChannelFilter();
		else if (epgStore.baseParams.channel !== r.data.key)
			epgStore.baseParams.channel = r.data.key;
        epgStore.reload();
    });

    epgFilterChannelTags.on('select', function(c, r) {
        if (r.data.identifier == -1)
            clearChannelTagsFilter();
		else if (epgStore.baseParams.tag !== r.data.name)
			epgStore.baseParams.tag = r.data.name;
        epgStore.reload();
	});
    
//IH 
// TODO - check what gets saved and where, and how we filter out null tages - may happen automatically because there's
// already a null tag. I think this only applies to tags, as they're saved to config
//
// Also, check that the insert method is genuinely inserting and not over-writing the first record.

    epgFilterContentGroup.on('select', function(c, r) {
        if (r.data.code == -1)
            clearContentGroupFilter();
		else if (epgStore.baseParams.contenttype !== r.data.code)
			epgStore.baseParams.contenttype = r.data.code;
        epgStore.reload();
    });

    epgFilterDuration.on('select', function(c, r) {
		if (r.data.identifier == -1)
            clearDurationFilter();
        else if (epgStore.baseParams.minduration !== r.data.minvalue) {
			epgStore.baseParams.minduration = r.data.minvalue;
            epgStore.baseParams.maxduration = r.data.maxvalue;
		}
		epgStore.reload();
    });
    
    epgFilterTitle.on('valid', function(c) {
        var value = c.getValue();

        if (value.length < 1)
            value = null;

        if (epgStore.baseParams.title !== value) {
            epgStore.baseParams.title = value;
            epgStore.reload();
        }
    });

    var epgView = new Ext.ux.grid.livegrid.GridView({
        nearLimit: 100,
        loadMask: {
            msg: 'Buffering. Please wait...'
        }
    });

    var panel = new Ext.ux.grid.livegrid.GridPanel({
        stateful: true,
        stateId: 'epggrid',
        enableDragDrop: false,
        cm: epgCm,
        plugins: [actions],
        title: 'Electronic Program Guide',
        iconCls: 'newspaper',
        store: epgStore,
        selModel: new Ext.ux.grid.livegrid.RowSelectionModel(),
        view: epgView,
        tbar: [
            epgFilterTitle,
            '-',
            epgFilterChannels,
            '-',
            epgFilterChannelTags,
            '-',
            epgFilterContentGroup,
            '-',
            epgFilterDuration,
            '-',
            {
                text: 'Reset All',
                handler: epgQueryClear
            },
            '->',
            {
                text: 'Watch TV',
                iconCls: 'eye',
                handler: function() {
                    new tvheadend.VideoPlayer();
                }
            },
            '-',
            {
                text: 'Create AutoRec',
                iconCls: 'wand',
                tooltip: 'Create an automatic recording entry that will '
                        + 'record all future programmes that matches '
                        + 'the current query.',
                handler: createAutoRec
            }, '-', {
                text: 'Help',
                handler: function() {
                    new tvheadend.help('Electronic Program Guide', 'epg.html');
                }
            }],
        bbar: new Ext.ux.grid.livegrid.Toolbar({
            view: epgView,
            displayInfo: true
        })
    });

    panel.on('rowclick', rowclicked);

    function rowclicked(grid, index) {
        new tvheadend.epgDetails(grid.getStore().getAt(index).data);
    }

    function createAutoRec() {

        var title = epgStore.baseParams.title ? epgStore.baseParams.title
                : "<i>Don't care</i>";
        var channel = epgStore.baseParams.channel ? tvheadend.channelLookupName(epgStore.baseParams.channel)
                : "<i>Don't care</i>";
        var tag = epgStore.baseParams.tag ? epgStore.baseParams.tag
                : "<i>Don't care</i>";
        var contenttype = epgStore.baseParams.contenttype ? tvheadend.contentGroupLookupName(epgStore.baseParams.contenttype)
                : "<i>Don't care</i>";
        var duration = epgStore.baseParams.minduration ? tvheadend.durationLookupRange(epgStore.baseParams.minduration)
                : "<i>Don't care</i>";


        Ext.MessageBox.confirm('Auto Recorder', 'This will create an automatic rule that '
                + 'continuously scans the EPG for programmes '
                + 'to record that match this query: ' + '<br><br>'
                + '<div class="x-smallhdr">Title:</div>' + title + '<br>'
                + '<div class="x-smallhdr">Channel:</div>' + channel + '<br>'
                + '<div class="x-smallhdr">Tag:</div>' + tag + '<br>'
                + '<div class="x-smallhdr">Genre:</div>' + contenttype + '<br>'
                + '<div class="x-smallhdr">Duration:</div>' + duration + '<br>'
                + '<br><br>' + 'Currently this will match (and record) '
                + epgStore.getTotalCount() + ' events. ' + 'Are you sure?',
            function(button) {
                if (button === 'no')
                    return;
                createAutoRec2(epgStore.baseParams);
            });
    }

    function createAutoRec2(params) {
        /* Really do it */
        params.op = 'createAutoRec';
        Ext.Ajax.request({
            url: 'dvr',
            params: params
        });
    }

    return panel;
};
