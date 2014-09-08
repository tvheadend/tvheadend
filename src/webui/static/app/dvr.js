/*
 * DVR Config / Schedule / Log editor and viewer
 */

/**
 *
 */
tvheadend.dvrDetails = function(uuid) {

    function showit(d) {
        var params = d[0].params;
        var chicon = params[0].value;
        var title = params[1].value;
        var desc = params[2].value;
        var status = params[3].value;
        var content = '';
        var but;

        if (chicon != null && chicon.length > 0)
            content += '<img class="x-epg-chicon" src="' + chicon + '">';

        content += '<div class="x-epg-title">' + title + '</div>';
        content += '<div class="x-epg-desc">' + desc + '</div>';
        content += '<hr>';
        content += '<div class="x-epg-meta">Status: ' + status + '</div>';

        var win = new Ext.Window({
            title: title,
            layout: 'fit',
            width: 400,
            height: 300,
            constrainHeader: true,
            buttonAlign: 'center',
            html: content
        });

        win.show();
    }

    tvheadend.loading(1);
    Ext.Ajax.request({
        url: 'api/idnode/load',
        params: {
            uuid: uuid,
            list: 'channel_icon,disp_title,disp_description,status',
        },
        success: function(d) {
            d = json_decode(d);
            tvheadend.loading(0),
            showit(d);
        },
        failure: function(d) {
            tvheadend.loading(0);
        }
    });
};

tvheadend.dvrRowActions = function() {
    return new Ext.ux.grid.RowActions({
        id: 'details',
        header: 'Details',
        width: 45,
        actions: [
            {
                iconIndex: 'sched_status'
            },
            {
                iconCls: 'info',
                qtip: 'Recording details',
                cb: function(grid, rec, act, row) {
                    new tvheadend.dvrDetails(grid.getStore().getAt(row).id);
                }
            }
        ],
        destroy: function() {
        }
    });
}

/**
 *
 */
tvheadend.dvr_upcoming = function(panel, index) {

    var actions = tvheadend.dvrRowActions();
    var list = 'disp_title,start,start_extra,stop,stop_extra,' +
               'channel,config_name';

    var abortButton = {
        name: 'abort',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Abort the selected recording',
                iconCls: 'cancel',
                text: 'Abort',
                disabled: true,
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.AjaxConfirm({
                    url: 'api/dvr/entry/cancel',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    },
                    question: 'Do you really want to abort/unschedule the selection?'
                });
            }
        }
    };

    function selected(s, abuttons) {
        var recording = 0;
        s.each(function(s) {
            if (s.data.sched_status == 'recording')
                recording++;
        });
        abuttons.abort.setDisabled(recording < 1);
    }

    function beforeedit(e, grid) {
        if (e.record.data.sched_status == 'recording')
            return false;
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_upcoming',
        titleS: 'Upcoming Recording',
        titleP: 'Upcoming Recordings',
        iconCls: 'clock',
        tabIndex: index,
        add: {
            url: 'api/dvr/entry',
            params: {
               list: list,
            },
            create: { }
        },
        edit: {
            params: {
                list: list,
            }
        },
        del: true,
        list: 'disp_title,episode,pri,start_real,stop_real,' +
              'duration,channelname,creator,config_name,' +
              'sched_status',
        sort: {
          field: 'start',
          direction: 'DESC'
        },
        plugins: [actions],
        lcol: [actions],
        tbar: [abortButton],
        selected: selected,
        beforeedit: beforeedit,
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        },
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_finished = function(panel, index) {

    var actions = tvheadend.dvrRowActions();

    var downloadButton = {
        name: 'download',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Download the selected recording',
                iconCls: 'save',
                text: 'Download',
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r.length > 0) {
              var url = r[0].data.url;
              window.location = url;
            }
        }
    };

    function selected(s, abuttons) {
        var count = s.getCount();
        abuttons.download.setDisabled(count < 1);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_finished',
        readonly: true,
        titleS: 'Finished Recording',
        titleP: 'Finished Recordings',
        iconCls: 'television',
        tabIndex: index,
        del: true,
        list: 'disp_title,episode,start_real,stop_real,' +
              'duration,filesize,channelname,creator,' +
              'sched_status,url',
        columns: {
            filesize: {
                renderer: function() {
                    return function(v) {
                        if (v == null)
                            return '';
                        return parseInt(v / 1000000) + ' MB';
                    }
                }
            }
        },
        sort: {
          field: 'start',
          direction: 'DESC'
        },
        plugins: [actions],
        lcol: [
            actions,
            {
                width: 40,
                header: "Play",
                renderer: function(v, o, r) {
                    var title = r.data['title'];
                    if (r.data['episode'])
                        title += ' / ' + r.data['episode'];
                    return '<a href="play/dvrfile/' + r.data['id'] +
                           '?title=' + encodeURIComponent(title) + '">Play</a>';
                }
            }],
        tbar: [downloadButton],
        selected: selected,
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        },
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_failed = function(panel, index) {

    var actions = tvheadend.dvrRowActions();

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_failed',
        comet: 'dvrentry',
        readonly: true,
        titleS: 'Failed Recording',
        titleP: 'Failed Recordings',
        iconCls: 'exclamation',
        tabIndex: index,
        del: true,
        list: 'disp_title,episode,start_real,stop_real,' +
              'duration,channelname,creator,' +
              'status,sched_status',
        sort: {
          field: 'start',
          direction: 'DESC'
        },
        plugins: [actions],
        lcol: [actions],
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        },
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_settings = function(panel, index) {
    tvheadend.idnode_form_grid(panel, {
        url: 'api/dvr/config',
        clazz: 'dvrconfig',
        comet: 'dvrconfig',
        titleS: 'Digital Video Recorder Profile',
        titleP: 'Digital Video Recorder Profiles',
        titleC: 'Profile Name',
        iconCls: 'drive',
        tabIndex: index,
        add: {
            url: 'api/dvr/config',
            create: { }
        },
        del: true,
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        },
    });

    return panel;

}

/**
 *
 */
tvheadend.autorec_editor = function(panel, index) {

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/autorec',
        titleS: 'DVR AutoRec Entry',
        titleP: 'DVR AutoRec Entries',
        iconCls: 'wand',
        tabIndex: index,
        columns: {
            enabled:      { width: 50 },
            title:        { width: 300 },
            channel:      { width: 200 },
            tag:          { width: 200 },
            content_type: { width: 100 },
            minduration:  { width: 80 },
            maxduration:  { width: 80 },
            weekdays:     { width: 160 },
            start:        { width: 100 },
            pri:          { width: 80 },
            config_name:  { width: 120 },
            creator:      { width: 200 },
            comment:      { width: 200 },
        },
        add: {
            url: 'api/dvr/autorec',
            params: {
               list: 'enabled,title,channel,tag,content_type,minduration,' +
                     'maxduration,weekdays,start,pri,config_name,comment',
            },
            create: { }
        },
        del: true,
        list: 'enabled,title,channel,tag,content_type,minduration,' +
              'maxduration,weekdays,start,pri,config_name,creator,comment',
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        },
    });

    return panel;

};

/**
 *
 */
tvheadend.dvr = function(panel, index) {
    var p = new Ext.TabPanel({
        activeTab: 0,
        autoScroll: true,
        title: 'Digital Video Recorder',
        iconCls: 'drive',
        items: [],
    });
    tvheadend.dvr_upcoming(p, 0);
    tvheadend.dvr_finished(p, 1);
    tvheadend.dvr_failed(p, 2);
    tvheadend.autorec_editor(p, 3);
    return p;
}
