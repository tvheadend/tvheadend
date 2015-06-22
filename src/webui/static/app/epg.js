insertContentGroupClearOption = function( scope, records, options ){
    var placeholder = Ext.data.Record.create(['val', 'key']);
    scope.insert(0,new placeholder({val: '(Clear filter)', key: '-1'}));
};

tvheadend.ContentGroupStore = tvheadend.idnode_get_enum({
    url: 'api/epg/content_type/list',
    listeners: {
        load: insertContentGroupClearOption
    }
});

tvheadend.contentGroupLookupName = function(code) {
    ret = "";
    if (!code)
        code = 0;
    code &= 0xf0;
    tvheadend.ContentGroupStore.each(function(r) {
        if (r.data.key === code)
            ret = r.data.val;
    });
    return ret;
};

tvheadend.ContentGroupFullStore = tvheadend.idnode_get_enum({
    url: 'api/epg/content_type/list',
    params: { full: 1 }
});

tvheadend.contentGroupFullLookupName = function(code) {
    ret = "";
    tvheadend.ContentGroupFullStore.each(function(r) {
        if (r.data.key === code)
            ret = r.data.val;
    });
    return ret;
};

tvheadend.channelLookupName = function(key) {
    channelString = "";

    var index = tvheadend.channels.find('key', key);

    if (index !== -1)
        var channelString = tvheadend.channels.getAt(index).get('val');

    return channelString;
};

tvheadend.channelTagLookupName = function(key) {
    tagString = "";

    var index = tvheadend.channelTags.find('key', key);

    if (index !== -1)
        var tagString = tvheadend.channelTags.getAt(index).get('val');

    return tagString;
};

// Store for duration filters - EPG, autorec dialog and autorec rules in the DVR grid
// NB: 'no max' is defined as 9999999s, or about 3 months...

tvheadend.DurationStore = new Ext.data.SimpleStore({
    storeId: 'durationnames',
    idIndex: 0,
    fields: ['identifier','label','minvalue','maxvalue'],
    data: [['-1', '(Clear filter)',"",""],
           ['1','00:00:00 - 00:15:00', 0, 900],
           ['2','00:15:00 - 00:30:00', 900, 1800],
           ['3','00:30:00 - 01:30:00', 1800, 5400],
           ['4','01:30:00 - 03:00:00', 5400, 10800],
           ['5','03:00:00 - No maximum', 10800, 9999999]]
});

// Function to convert numeric duration to corresponding label string
// Note: triggered by minimum duration only. This would fail if ranges
// had the same minimum (e.g. 15-30 mins and 15-60 minutes) (which we don't have). 

tvheadend.durationLookupRange = function(value) {
    durationString = "";
    var index = tvheadend.DurationStore.find('minvalue', value);
    if (index !== -1)
        var durationString = tvheadend.DurationStore.getAt(index).data.label;

    return durationString;
};

tvheadend.epgDetails = function(event) {

    var content = '';
    var duration = 0;

    if (event.start && event.stop && event.stop - event.start > 0)
        duration = (event.stop - event.start) / 1000;

    if (event.channelIcon != null && event.channelIcon.length > 0)
        content += '<img class="x-epg-chicon" src="' + event.channelIcon + '">';

    content += '<div class="x-epg-title">' + event.title;
    if (event.subtitle)
        content += "&nbsp;:&nbsp;" + event.subtitle;
    content += '</div>';
    if (event.episodeOnscreen)
        content += '<div class="x-epg-title">' + event.episodeOnscreen + '</div>';
    if (event.start)
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('Start Time') + ':</div> ' + tvheadend.niceDate(event.start) + '</div>';
    if (event.stop)
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('End Time') + ':</div> ' + tvheadend.niceDate(event.stop) + '</div>';
    if (duration)
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('Duration') + ':</div> ' + parseInt(duration / 60) + ' min</div>';
    if (event.summary)
      content += '<div class="x-epg-summary">' + event.summary + '</div>';
    if (event.description)
      content += '<div class="x-epg-desc">' + event.description + '</div>';
    if (event.starRating || event.ageRating || event.genre)
      content += '<hr/>';
    if (event.starRating)
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('Star Rating') + ':</div> ' + event.starRating + '</div>';
    if (event.ageRating)
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('Age Rating') + ':</div> ' + event.ageRating + '</div>';
    if (event.genre) {
      var genre = [];
      Ext.each(event.genre, function(g) {
        var g1 = tvheadend.contentGroupLookupName(g);
        var g2 = tvheadend.contentGroupFullLookupName(g);
        if (g1 == g2)
          g1 = '';
        if (g1 || g2)
          genre.push((g1 ? '[' + g1 + '] ' : '') + g2);
      });
      content += '<div class="x-epg-meta"><div class="x-epg-prefix">' + _('Content Type') + ':</div> ' + genre.join(', ') + '</div>';
    }

    content += '<div id="related"></div>';
    content += '<div id="altbcast"></div>';
    
    var now = new Date();
    var buttons = [];
    var recording = event.dvrState.indexOf('recording') === 0;
    var scheduled = event.dvrState.indexOf('scheduled') === 0;

    if (!recording && !scheduled) {
        buttons.push(new Ext.Button({
            disabled: !event.title,
            handler: searchIMDB,
            iconCls: 'find',
            tooltip: _('Search IMDB (for title)'),
            text: _("Search IMDB")
        }));
    }

    buttons.push(new Ext.Button({
        disabled: event.start > now || event.stop < now,
        handler: playProgram,
        iconCls: 'control_play',
        tooltip: _('Play this program'),
        text: _("Play program")
    }));

    if (tvheadend.accessUpdate.dvr) {

        var store = new Ext.data.JsonStore({
            autoload: true,
            root: 'entries',
            fields: ['key','val'],
            id: 'key',
            url: 'api/idnode/load',
            baseParams: {
                'enum': 1,
                'class': 'dvrconfig'
            },
            sortInfo: {
                field: 'val',
                direction: 'ASC'
            }
        });
        store.load();

        if (recording) {
          buttons.push(new Ext.Button({
              handler: stopDVR,
              iconCls: 'stopRec',
              tooltip: _('Stop recording of this program'),
              text: _("Stop recording")
          }));
        }

        if (scheduled) {
          buttons.push(new Ext.Button({
              handler: deleteDVR,
              iconCls: 'remove',
              tooltip: _('Delete scheduled recording of this program'),
              text: _("Delete recording")
          }));
        }

        var confcombo = new Ext.form.ComboBox({
            store: store,
            triggerAction: 'all',
            mode: 'local',
            valueField: 'key',
            displayField: 'val',
            name: 'config_name',
            emptyText: _('(default DVR Profile)'),
            value: '',
            editable: false
        });

        buttons.push(confcombo);
        buttons.push(new Ext.Button({
            handler: recordEvent,
            iconCls: 'rec',
            tooltip: _('Record now this program'),
            text: _('Record program')
        }));
        buttons.push(new Ext.Button({
            handler: recordSeries,
            iconCls: 'autoRec',
            tooltip: _('Create an automatic recording entry for this program that will '
                 + 'record all future programs that match '
                 + 'the current query.'),
            text: event.serieslinkId ? _("Record series") : _("Autorec")
        }));

    } else {

        buttons.push(new Ext.Button({
            handler: function() { win.close(); },
            text: "Close"
        }));

    }

    var win = new Ext.Window({
        title: _('Broadcast Details'),
        iconCls: 'broadcast_details',
        layout: 'fit',
        width: 650,
        height: 450,
        constrainHeader: true,
        buttons: buttons,
        buttonAlign: 'center',
        autoScroll: true,
        html: content
    });
    win.show();

    function searchIMDB() {
        window.open('http://akas.imdb.com/find?q=' +
                    encodeURIComponent(event.title), '_blank');
    }

    function playProgram() {
        var title = event.title;
        if (event.episodeOnscreen)
          title += ' / ' + event.episodeOnscreen;
        window.open('play/stream/channel/' + event.channelUuid +
                    '?title=' + encodeURIComponent(title), '_blank');
    }

    function recordEvent() {
        record('api/dvr/entry/create_by_event');
    }

    function recordSeries() {
        record('api/dvr/autorec/create_by_series');
    }

    function stopDVR() {
        tvheadend.AjaxConfirm({
            url: 'api/dvr/entry/stop',
            params: {
                uuid: event.dvrUuid
            },
            success: function(d) {
                win.close();
            },
            question: _('Do you really want to gracefully stop/unschedule this recording?')
        });
    }

    function deleteDVR() {
        tvheadend.AjaxConfirm({
            url: 'api/idnode/delete',
            params: {
                uuid: event.dvrUuid
            },
            success: function(d) {
                win.close();
            },
            question: _('Do you really want to remove this recording?')
        });
    }

    function record(url) {
        Ext.Ajax.request({
            url: url,
            params: {
                event_id: event.eventId,
                config_uuid: confcombo.getValue()
            },
            success: function(response, options) {
                win.close();
            },
            failure: function(response, options) {
                Ext.MessageBox.alert(_('DVR'), response.statusText);
            }
        });
    }
};

tvheadend.epg = function() {
    var lookup = '<span class="x-linked">&nbsp;</span>';

    var detailsfcn = function(grid, rec, act, row) {
        new tvheadend.epgDetails(grid.getStore().getAt(row).data);
    };

    var actions = new Ext.ux.grid.RowActions({
        id: 'details',
        header: _('Details'),
        width: 45,
        dataIndex: 'actions',
        callbacks: {
            'recording':      detailsfcn,
            'recordingError': detailsfcn,
            'scheduled':      detailsfcn,
            'completed':      detailsfcn,
            'completedError': detailsfcn
        },
        actions: [
            {
                iconCls: 'broadcast_details',
                qtip: _('Broadcast details'),
                cb: detailsfcn
            },
            {
                iconIndex: 'dvrState'
            }
                                                                                                          
        ]
    });

    var epgStore = new Ext.ux.grid.livegrid.Store({
        autoLoad: true,
        url: 'api/epg/events/grid',
        bufferSize: 300,
        reader: new Ext.ux.grid.livegrid.JsonReader({
            root: 'entries',
            totalProperty: 'totalCount',
            id: 'eventId'
        },
        [
            { name: 'eventId' },
            { name: 'channelName' },
            { name: 'channelUuid' },
            { name: 'channelNumber' },
            { name: 'channelIcon' },
            { name: 'title' },
            { name: 'subtitle' },
            { name: 'summary' },
            { name: 'description' },
            { name: 'episodeOnscreen' },
            {
                name: 'start',
                type: 'date',
                dateFormat: 'U' /* unix time */
            },
            {
                name: 'stop',
                type: 'date',
                dateFormat: 'U' /* unix time */
            },
            { name: 'starRating' },
            { name: 'ageRating' },
            { name: 'genre' },
            { name: 'dvrUuid' },
            { name: 'dvrState' },
            { name: 'serieslinkId' }
        ])
    });

    function setMetaAttr(meta, record, cursor) {
        var now = new Date;
        var start = record.get('start');
        var extra = cursor ? 'cursor:alias;' : '';

        if (now.getTime() >= start.getTime())
            meta.attr = 'style="font-weight:bold;' + extra + '"';
        else if (extra)
            meta.attr = 'style="' + extra + '"';
    }

    function renderDate(value, meta, record) {
        setMetaAttr(meta, record);

        if (value) {
          var dt = new Date(value);
          return dt.format('D, M d, H:i');
        }
        return "";
    }

    function renderDuration(value, meta, record) {
        setMetaAttr(meta, record);

        value = record.data.stop - record.data.start;
        if (!value || value < 0)
            value = 0;

        value = Math.floor(value / 60000);

        if (value >= 60) {
            var min = value % 60;
            var hours = Math.floor(value / 60);

            if (min === 0) {
                return hours + ' ' + _('hrs');
            }
            return hours + ' ' + _('hrs') + ', ' + min + ' ' + _('min');
        }
        else {
            return value + ' ' + _('min');
        }
    }

    function renderText(value, meta, record) {
        setMetaAttr(meta, record);

        return value;
    }

    function renderTextLookup(value, meta, record) {
        setMetaAttr(meta, record, value);
        if (!value) return "";
        return lookup + value;
    }

    function renderInt(value, meta, record) {
        setMetaAttr(meta, record);

        return '' + value;
    }

    var epgCm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns: [
            actions,
            new Ext.ux.grid.ProgressColumn({
                width: 100,
                header: _("Progress"),
                dataIndex: 'progress',
                colored: false,
                ceiling: 100,
                timeout: 20000, // 20 seconds
                tvh_renderer: function(value, meta, record) {
                    var entry = record.data;
                    var start = entry.start;           // milliseconds
                    var duration = entry.stop - start; // milliseconds
                    var now = new Date();

                    if (!duration || duration < 0) duration = 0;
                    // Only render a progress bar for currently running programmes
                    if (now >= start && now - start <= duration)
                        return (now - start) / duration * 100;
                    else
                        return "";
                }
            }),
            {
                width: 250,
                id: 'title',
                header: _("Title"),
                dataIndex: 'title',
                renderer: function(value, meta, record) {
                    var clickable = tvheadend.regexEscape(record.data['title']) !=
                                    epgStore.baseParams.title;
                    setMetaAttr(meta, record, value && clickable);
                    return !value ? '' : (clickable ? lookup : '') + value;
                },
                listeners: { click: { fn: clicked } }
            },
            {
                width: 250,
                id: 'subtitle',
                header: _("SubTitle"),
                dataIndex: 'subtitle',
                renderer: renderText
            },
            {
                width: 100,
                id: 'episodeOnscreen',
                header: _("Episode"),
                dataIndex: 'episodeOnscreen',
                renderer: renderText
            },
            {
                width: 100,
                id: 'start',
                header: _("Start Time"),
                dataIndex: 'start',
                renderer: renderDate
            },
            {
                width: 100,
                hidden: true,
                id: 'stop',
                header: _("End Time"),
                dataIndex: 'stop',
                renderer: renderDate
            },
            {
                width: 100,
                id: 'duration',
                header: _("Duration"),
                renderer: renderDuration
            },
            {
                width: 60,
                id: 'channelNumber',
                header: _("Number"),
                align: 'right',
                dataIndex: 'channelNumber',
                renderer: renderText
            },
            {
                width: 250,
                id: 'channelName',
                header: _("Channel"),
                dataIndex: 'channelName',
                renderer: function(value, meta, record) {
                    var clickable = record.data['channelUuid'] !==
                                    epgStore.baseParams.channel;
                    setMetaAttr(meta, record, value && clickable);
                    return !value ? '' : (clickable ? lookup : '') + value;
                },
                listeners: { click: { fn: clicked } }
            },
            {
                width: 50,
                id: 'starRating',
                header: _("Stars"),
                dataIndex: 'starRating',
                renderer: renderInt
            },
            {
                width: 50,
                id: 'ageRating',
                header: _("Age"),
                dataIndex: 'ageRating',
                renderer: renderInt
            }, {
                width: 250,
                id: 'genre',
                header: _("Content Type"),
                dataIndex: 'genre',
                renderer: function(vals, meta, record) {
                    var r = [];
                    Ext.each(vals, function(v) {
                        v = tvheadend.contentGroupFullLookupName(v);
                        if (v)
                          r.push(v);
                    });
                    var clickable = false;
                    if (r.length > 0 && vals[0]) {
                        var v = vals[0] & 0xf0;
                        clickable = v !== epgStore.baseParams.contentType;
                    }
                    setMetaAttr(meta, record, clickable);
                    if (r.length < 1) return "";
                    return (clickable ? lookup : '') + r.join(',');
                },
                listeners: { click: { fn: clicked } }
            }
        ]
    });

    var filter = new Ext.ux.grid.GridFilters({
        encode: true,
        local: false,
        filters: [
            { type: 'string',   dataIndex: 'title' },
            { type: 'string',   dataIndex: 'subtitle' },
            { type: 'string',   dataIndex: 'episodeOnscreen' },
            { type: 'intsplit', dataIndex: 'channelNumber', intsplit: 1000000 },
            { type: 'string',   dataIndex: 'channelName' },
            { type: 'numeric',  dataIndex: 'starRating' },
            { type: 'numeric',  dataIndex: 'ageRating' }
        ]
    });

    // Title search box

    var epgFilterTitle = new Ext.form.TextField({
        emptyText: _('Search title...'),
        width: 200
    });

    var epgFilterFulltext = new Ext.form.Checkbox({
        width: 20
    });

    // Channels, uses global store

    var epgFilterChannels = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 200,
        displayField: 'val',
        store: tvheadend.channels,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        typeAhead: true,
        emptyText: _('Filter channel...'),
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearChannelFilter();
                    epgView.reset();
                }
            }
        }
    });

    // Tags, uses global store

    var epgFilterChannelTags = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 200,
        displayField: 'val',
        store: tvheadend.channelTags,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        typeAhead: true,
        emptyText: _('Filter tag...'),
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearChannelTagsFilter();
                    epgView.reset();
                }
            }
        }

    });

    // Content groups

    var epgFilterContentGroup = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 200,
        displayField: 'val',
        store: tvheadend.ContentGroupStore,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        typeAhead: true,
        emptyText: _('Filter content type...'),
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearContentGroupFilter();
                    epgView.reset();
                }
            }
        }
    });

    var epgFilterDuration = new Ext.form.ComboBox({
        loadingText: _('Loading...'),
        width: 150,
        displayField: 'label',
        store: tvheadend.DurationStore,
        mode: 'local',
        editable: true,
        forceSelection: true,
        triggerAction: 'all',
        typeAhead: true,
        emptyText: _('Filter duration...'),
        listeners: {
            blur: function () {
                if(this.getRawValue() == "" ) {
                    clearDurationFilter();
                    epgView.reset();
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

    clearFulltextFilter = function() {
        delete epgStore.baseParams.fulltext;
        epgFilterFulltext.setValue(0);
    };

    clearChannelFilter = function() {
        delete epgStore.baseParams.channel;
        epgFilterChannels.setValue("");
    };

    clearChannelTagsFilter = function() {
        delete epgStore.baseParams.channelTag;
        epgFilterChannelTags.setValue("");
    };

    clearContentGroupFilter = function() {
        delete epgStore.baseParams.contentType;
        epgFilterContentGroup.setValue("");
    };

    clearDurationFilter = function() {
        delete epgStore.baseParams.durationMin;
        delete epgStore.baseParams.durationMax;
        epgFilterDuration.setValue("");
    };

    function epgQueryClear() {
        clearTitleFilter();
        clearFulltextFilter();
        clearChannelFilter();
        clearChannelTagsFilter();
        clearDurationFilter();
        clearContentGroupFilter();
        filter.clearFilters();
        delete epgStore.sortInfo;
        epgView.reset();
    };

/*
 * Filter selection event handlers
 */

    function epgFilterChannelSet(val) {
        if (!val)
            clearChannelFilter();
        else if (epgStore.baseParams.channel !== val)
            epgStore.baseParams.channel = val;
        epgView.reset();
    }

    epgFilterChannels.on('select', function(c, r) {
        epgFilterChannelSet(r.data.key == -1 ? "" : r.data.key);
    });

    epgFilterChannelTags.on('select', function(c, r) {
        if (r.data.key == -1)
            clearChannelTagsFilter();
        else if (epgStore.baseParams.channelTag !== r.data.key)
            epgStore.baseParams.channelTag = r.data.key;
        epgView.reset();
    });

    function epgFilterContentGroupSet(val) {
        if (!val)
            clearContentGroupFilter();
        else if (epgStore.baseParams.contentType !== val)
            epgStore.baseParams.contentType = val;
        epgView.reset();
    }

    epgFilterContentGroup.on('select', function(c, r) {
        epgFilterContentGroupSet(r.data.key == -1 ? "" : r.data.key);
    });

    epgFilterDuration.on('select', function(c, r) {
        if (r.data.identifier == -1)
            clearDurationFilter();
        else if (epgStore.baseParams.durationMin !== r.data.minvalue) {
            epgStore.baseParams.durationMin = r.data.minvalue;
            epgStore.baseParams.durationMax = r.data.maxvalue;
        }
        epgView.reset();
    });

    epgFilterTitle.on('valid', function(c) {
        var value = c.getValue();

        if (value.length < 1)
            value = null;

        if (epgStore.baseParams.title !== value) {
            epgStore.baseParams.title = value;
            epgView.reset();
        }
    });

    epgFilterFulltext.on('check', function(c, value) {
        if (epgStore.baseParams.fulltext !== value) {
            epgStore.baseParams.fulltext = value;
            epgView.reset();
        }
    });

    var epgView = new Ext.ux.grid.livegrid.GridView({
        nearLimit: 100,
        loadMask: {
            msg: _('Buffering. Please wait...')
        },
        listeners: {
            beforebuffer: {
                fn: function(view, ds, index, range, total, options) {
                    /* filters hack */
                    filter.onBeforeLoad(ds, options);
                }
            }
        }
    });

    tvheadend.autorecButton = new Ext.Button({
        text: _('Create AutoRec'),
        iconCls: 'autoRec',
        tooltip: _('Create an automatic recording entry that will '
                 + 'record all future programs that match '
                 + 'the current query.'),
        handler: createAutoRec
    });

    var tbar = [
        epgFilterTitle, { text: _('Fulltext') }, epgFilterFulltext, '-',
        epgFilterChannels, '-',
        epgFilterChannelTags, '-',
        epgFilterContentGroup, '-',
        epgFilterDuration, '-',
        {
            text: _('Reset All'),
            iconCls: 'resetIcon',
            tooltip: _('Reset all filters (show all)'),
            handler: epgQueryClear
        },
        '->',
        {
            text: _('Watch TV'),
            iconCls: 'watchTv',
            tooltip: _('Watch TV online in the window by web'),
            handler: function() {
                new tvheadend.VideoPlayer();
            }
        },
        '-',
        tvheadend.autorecButton,
        '-',
        {
            text: _('Help'),
            iconCls: 'help',
            handler: function() {
                new tvheadend.help(_('Electronic Program Guide'), 'epg.html');
            }
        }
    ];

    var panel = new Ext.ux.grid.livegrid.GridPanel({
        stateful: true,
        stateId: 'epggrid',
        enableDragDrop: false,
        cm: epgCm,
        plugins: [filter, actions],
        title: _('Electronic Program Guide'),
        iconCls: 'epg',
        store: epgStore,
        selModel: new Ext.ux.grid.livegrid.RowSelectionModel(),
        view: epgView,
        tbar: tbar,
        bbar: new Ext.ux.grid.livegrid.Toolbar(
            tvheadend.PagingToolbarConf({view: epgView},_('Events'),0,0)
        ),
        listeners: {
            beforestaterestore: {
               fn: function(grid, state) {
                   /* do not restore sorting and filters */
                   state.sort = {};
                   state.filters = {};
               }
            }
        }
    });

    panel.on('rowclick', rowclicked);
    panel.on('filterupdate', function() {
        epgView.reset();
    });

    /**
     * Listener for EPG and DVR notifications.
     * We want to update the EPG grid when a recording is finished/deleted etc.
     * so the status icon gets updated. Only do this when the tab is visible,
     * otherwise it won't work as expected.
     */
    tvheadend.comet.on('epg', function(m) {
        if (!panel.isVisible())
            return;
        if ('delete' in m) {
            for (var i = 0; i < m['delete'].length; i++) {
                var r = epgStore.getById(m['delete'][i]);
                if (r)
                  epgStore.remove(r);
            }
        }
        if (m.update || m.dvr_update || m.dvr_delete) {
            var a = m.update || m.dvr_update || m.dvr_delete;
            if (m.update && m.dvr_update)
                var a = m.update.concat(m.dvr_update);
            if (m.update || m.dvr_update)
                a = a.concat(m.dvr_delete);
            var ids = [];
            for (var i = 0; i < a.length; i++) {
                var r = epgStore.getById(a[i]);
                if (r)
                  ids.push(r.id);
            }
            if (ids) {
                Ext.Ajax.request({
                    url: 'api/epg/events/load',
                    params: {
                        eventId: ids
                    },
                    success: function(d) {
                        d = json_decode(d);
                        for (var i = 0; i < d.length; i++) {
                            var r = epgStore.getById(d[i].eventId);
                            if (r) {
                                for (var j = 0; j < r.store.fields.items.length; j++) {
                                    var n = r.store.fields.items[j];
                                    var v = d[i][n.name];
                                    r.data[n.name] = n.convert((v !== undefined) ? v : n.defaultValue, v);
                                }
                                r.json = d[i];
                                r.commit();
                            }
                        }
                    },
                    failure: function(response, options) {
                        Ext.MessageBox.alert(_('EPG Update'), response.statusText);
                    }
                });
            }
        }
    });
    
    // Always reload the store when the tab is activated
    panel.on('beforeshow', function() {
        epgStore.reload();
    });

    function clicked(column, grid, index, e) {
        if (column.dataIndex === 'title') {
            var value = grid.getStore().getAt(index).data[column.dataIndex];
            value = tvheadend.regexEscape(value);
            if (value && epgStore.baseParams.title !== value) {
                epgFilterTitle.setValue(value);
                return false;
            }
        } else if (column.dataIndex === 'channelName') {
            var rec = grid.getStore().getAt(index).data;
            var value = rec['channelUuid'];
            if (value && epgStore.baseParams.channel !== value) {
                epgFilterChannels.setValue(rec['channelName']);
                epgFilterChannelSet(value);
                return false;
            }
        } else if (column.dataIndex === 'genre') {
            var value = grid.getStore().getAt(index).data[column.dataIndex];
            if (value && value.length > 0) {
                value = parseInt(value[0]) & 0xf0;
                if (value && epgStore.baseParams.channelTag !== value) {
                    var l = tvheadend.contentGroupLookupName(value);
                    epgFilterContentGroup.setValue(l);
                    epgFilterContentGroupSet(value);
                    return false;
                }
            }
        }
    }

    function rowclicked(grid, index, e) {
        new tvheadend.epgDetails(grid.getStore().getAt(index).data);
    }

    function createAutoRec() {
    
        if (!tvheadend.accessUpdate.dvr)
            return;

        var title = epgStore.baseParams.title ?
                epgStore.baseParams.title
                : "<i>" + _("Don't care") + "</i>";
        var fulltext = epgStore.baseParams.fulltext ?
                " <i>(" + _("Fulltext") + ")</i>"
                : "";
        var channel = epgStore.baseParams.channel ?
                tvheadend.channelLookupName(epgStore.baseParams.channel)
                : "<i>" + _("Don't care") + "</i>";
        var tag = epgStore.baseParams.channelTag ?
                tvheadend.channelTagLookupName(epgStore.baseParams.channelTag)
                : "<i>" + _("Don't care") + "</i>";
        var contentType = epgStore.baseParams.contentType ?
                tvheadend.contentGroupLookupName(epgStore.baseParams.contentType)
                : "<i>" + _("Don't care") + "</i>";
        var duration = epgStore.baseParams.durationMin ?
                tvheadend.durationLookupRange(epgStore.baseParams.durationMin)
                : "<i>" + _("Don't care") + "</i>";

        Ext.MessageBox.confirm(_('Auto Recorder'), _('This will create an automatic rule that '
                + 'continuously scans the EPG for programs '
                + 'to record that match this query') + ': ' + '<br><br>'
                + '<div class="x-smallhdr">' + _('Title') + ':</div>' + title + fulltext + '<br>'
                + '<div class="x-smallhdr">' + _('Channel') + ':</div>' + channel + '<br>'
                + '<div class="x-smallhdr">' + _('Tag') + ':</div>' + tag + '<br>'
                + '<div class="x-smallhdr">' + _('Genre') + ':</div>' + contentType + '<br>'
                + '<div class="x-smallhdr">' + _('Duration') + ':</div>' + duration + '<br>'
                + '<br><br>'
                + sprintf(_('Currently this will match (and record) %d events.'), epgStore.GetTotalCount())
                + ' ' + 'Are you sure?',
            function(button) {
                if (button === 'no')
                    return;
                createAutoRec2(epgStore.baseParams);
            });
    }

    function createAutoRec2(params) {
        /* Really do it */
        var conf = {
          enabled: 1,
          comment: _('Created from EPG query')
        };
        if (params.title) conf.title = params.title;
        if (params.fulltext) conf.fulltext = params.fulltext;
        if (params.channel) conf.channel = params.channel;
        if (params.channelTag) conf.tag = params.channelTag;
        if (params.contentType) conf.content_type = params.contentType;
        if (params.durationMin) conf.minduration = params.durationMin;
        if (params.durationMax) conf.maxduration = params.durationMax;
        Ext.Ajax.request({
            url: 'api/dvr/autorec/create',
            params: { conf: Ext.encode(conf) }
        });
    }

    return panel;
};
