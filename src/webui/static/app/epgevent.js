/*
 * epgevent.js
 * EPG dialogs for broadcast events.
 * Copyright (C) 2018 Tvheadend Foundation CIC
 */

/// Display dialog showing alternative showings for a broadcast event.
/// @param alternative - If true then display "alternatives", otherwise display "related" broadcasts
function epgAlternativeShowingsDialog(eventId, alternative) {
    // Default params only exist in ECMA2015+, so do it old way.
    alternative = (typeof alternative !== 'undefined') ?  alternative : true;

    function getAlternativeShowingsStore(eventId) {
        var base = alternative ? "alternative" : "related";
        return new Ext.ux.grid.livegrid.Store({
            autoLoad: true,
            // Passing params doesn't seem to work, so force eventId in to url.
            url: 'api/epg/events/' + base + '?eventId='+eventId,
            baseParams: {
                eventId: eventId,
            },
            bufferSize: 300,
            selModel: new Ext.ux.grid.livegrid.RowSelectionModel(),
            reader: new Ext.ux.grid.livegrid.JsonReader({
                root: 'entries',
                totalProperty: 'totalCount',
                id: 'eventId'
            }, [
                // We need a complete set of fields since user can request
                // dialog that retrieves its data from our store.
                { name: 'eventId' },
                { name: 'channelName' },
                { name: 'channelUuid' },
                { name: 'channelNumber' },
                { name: 'channelIcon' },
                { name: 'title' },
                { name: 'subtitle' },
                { name: 'summary' },
                { name: 'description' },
                { name: 'extratext' },
                { name: 'episodeOnscreen' },
                { name: 'image' },
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
                {
                    name: 'first_aired',
                    type: 'date',
                    dateFormat: 'U' /* unix time */
                },
                { name: 'duration' },
                { name: 'starRating' },
                { name: 'credits' },
                { name: 'category' },
                { name: 'keyword' },
                { name: 'ageRating' },
                { name: 'copyright_year' },
                { name: 'new' },
                { name: 'genre' },
                { name: 'dvrUuid' },
                { name: 'dvrState' },
                { name: 'serieslinkUri' }
            ])
        });
    }                           //getAlternativeShowingsStore


    var store = getAlternativeShowingsStore(eventId);
    var epgView = new Ext.ux.grid.livegrid.GridView({
        nearLimit: 100,
        loadMask: {
            msg: _('Buffering. Please wait…')
        },
    });

    var epgEventDetails = getEPGEventDetails();

    var grid = new Ext.ux.grid.livegrid.GridPanel({
        store: store,
        plugins: [epgEventDetails],
        iconCls: 'epg',
        view: epgView,
        cm: new Ext.grid.ColumnModel({
            columns: [
                epgEventDetails,
                {
                    width: 250,
                    id: 'title',
                    header: _("Title"),
                    tooltip: _("Title"),
                    dataIndex: 'title',
                },
                {
                    width: 250,
                    id: 'extratext',
                    header: _("Extra text"),
                    tooltip: _("Extra text: subtitle or summary or description"),
                    dataIndex: 'extratext',
                    renderer: tvheadend.renderExtraText
                },
                {
                    width: 100,
                    id: 'episodeOnscreen',
                    header: _("Episode"),
                    tooltip: _("Episode"),
                    dataIndex: 'episodeOnscreen',
                },
                {
                    width: 200,
                    id: 'start',
                    header: _("Start Time"),
                    tooltip: _("Start Time"),
                    dataIndex: 'start',
                    renderer: tvheadend.renderCustomDate
                },
                {
                    width: 200,
                    id: 'stop',
                    header: _("End Time"),
                    tooltip: _("End Time"),
                    dataIndex: 'stop',
                    renderer: tvheadend.renderCustomDate
                },
                {
                    width: 250,
                    id: 'channelName',
                    header: _("Channel"),
                    tooltip: _("Channel"),
                    dataIndex: 'channelName',
                },
            ],
        }),
    });                      // grid


    var windowHeight = Ext.getBody().getViewSize().height - 150;

    var win = new Ext.Window({
        title: alternative ? _('Alternative Showings') : _('Related Showings'),
        iconCls: 'info',
        layout: 'fit',
        width: 1317,
        height: windowHeight,
        constrainHeader: true,
        buttonAlign: 'center',
        autoScroll: false,  // Internal grid has its own scrollbars so no need for us to have them
        items: grid,
        bbar: new Ext.ux.grid.livegrid.Toolbar(
        tvheadend.PagingToolbarConf({view: epgView},_('Events'),0,0)
        ),

    });

    // Handle comet updates until user closes dialog.
    var update = function(m) {
        tvheadend.epgCometUpdate(m, store);
    };
    tvheadend.comet.on('epg', update);
    win.on('close', function(panel, opts) {
         tvheadend.comet.un('epg', update);
     });

    win.show();
}


var epgAlternativeShowingsDialogForSelection = function(conf, e, store, select, alternative) {
    var r = select.getSelections();
    if (r && r.length > 0) {
        for (var i = 0; i < r.length; i++) {
            var rec = r[i];
            var eventId = rec.data['broadcast'];
            if (eventId)
                epgAlternativeShowingsDialog(eventId, alternative);
        }
    }
};

var epgShowRelatedButtonConf = {
    name: 'epgrelated',
    builder: function() {
        return new Ext.Toolbar.Button({
            tooltip: _('Display dialog of related broadcasts'),
            iconCls: 'epgrelated',
            text: _('Related broadcasts'),
            disabled: true
        });
    },
    callback: function(conf, e, store, select) {
        epgAlternativeShowingsDialogForSelection(conf, e, store, select, false);
    }
}

var epgShowAlternativesButtonConf = {
    name: 'epgalt',
    builder: function() {
        return new Ext.Toolbar.Button({
            tooltip: _('Display dialog showing alternative broadcasts'),
            iconCls: 'duprec',
            text: _('Alternative showings'),
            disabled: true
        });
    },
    callback: function(conf, e, store, select) {
        epgAlternativeShowingsDialogForSelection(conf, e, store, select, true);
    }
};
