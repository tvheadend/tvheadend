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
        var episode = params[2].value;
        var start_real = params[3].value;
        var stop_real = params[4].value;
        var duration = params[5].value;
        var desc = params[6].value;
        var status = params[7].value;
        var filesize = params[8].value;
        var content = '';
        var but;

        if (chicon != null && chicon.length > 0)
            content += '<img class="x-epg-chicon" src="' + chicon + '">';

        content += '<div class="x-epg-title">' + title + '</div>';
        content += '<div class="x-epg-title">' + episode + '</div>';
        content += '<div class="x-epg-time"><div class="x-epg-prefix">Scheduled Start Time:</div> ' + tvheadend.niceDate(start_real * 1000) + '</div>';
        content += '<div class="x-epg-time"><div class="x-epg-prefix">Scheduled Stop Time:</div> ' + tvheadend.niceDate(stop_real * 1000) + '</div>';
        content += '<div class="x-epg-time"><div class="x-epg-prefix">Duration:</div> ' + parseInt(duration / 60) + ' min</div>';
        content += '<div class="x-epg-desc">' + desc + '</div>';
        content += '<hr>';
        content += '<div class="x-epg-meta"><div class="x-epg-prefix">Status:</div> ' + status + '</div>';
        content += '<div class="x-epg-meta"><div class="x-epg-prefix">File size:</div> ' + parseInt(filesize / 1000000) + ' MB</div>';

        var win = new Ext.Window({
            title: title,
            iconCls: 'info',
            layout: 'fit',
            width: 500,
            height: 400,
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
            list: 'channel_icon,disp_title,episode,start_real,stop_real,' +
                  'duration,disp_description,status,filesize'
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

tvheadend.weekdaysRenderer = function(st) {
    return function(v) {
        var t = [];
        var d = v.push ? v : [v];
        if (d.length == 7) {
            v = "All days";
        } else if (d.length == 0) {
            v = "No days";
        } else {
            for (var i = 0; i < d.length; i++) {
                 var r = st.find('key', d[i]);
                 if (r !== -1) {
                     var nv = st.getAt(r).get('val');
                     if (nv)
                         t.push(nv);
                 } else {
                     t.push(d[i]);
                 }
            }
            v = t.join(',');
        }
        return v;
    }
}

tvheadend.filesizeRenderer = function(st) {
    return function() {
        return function(v) {
            if (v == null)
                return '';
            if (!v || v < 0)
                return '---';
            if (v > 1000000)
                return parseInt(v / 1000000) + ' MB';
            if (v > 1000)
                return parseInt(v / 1000) + ' KB';
            return parseInt(v) + ' B';
        }
    }
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
                iconCls: 'abort',
                text: 'Abort',
                disabled: true
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
            if (s.data.sched_status.indexOf('recording') == 0)
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
        titleP: 'Upcoming / Current Recordings',
        iconCls: 'upcomingRec',
        tabIndex: index,
        add: {
            url: 'api/dvr/entry',
            params: {
               list: list
            },
            create: { }
        },
        edit: {
            params: {
                list: list
            }
        },
        del: true,
        list: 'disp_title,episode,pri,start_real,stop_real,' +
              'duration,channelname,creator,config_name,' +
              'sched_status',
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
        lcol: [actions],
        tbar: [abortButton],
        selected: selected,
        beforeedit: beforeedit,
        help: function() {
            new tvheadend.help('DVR-Upcoming/Current Recordings', 'dvr_upcoming.html');
        }
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
                iconCls: 'download',
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
        var r = s.getSelections();
        var b = r.length > 0 && r[0].data.filesize > 0;
        abuttons.download.setDisabled(!b);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_finished',
        readonly: true,
        titleS: 'Finished Recording',
        titleP: 'Finished Recordings',
        iconCls: 'finishedRec',
        tabIndex: index,
        del: true,
        list: 'disp_title,episode,start_real,stop_real,' +
              'duration,filesize,channelname,creator,' +
              'sched_status,url',
        columns: {
            filesize: {
                renderer: tvheadend.filesizeRenderer()
            }
        },
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
        lcol: [
            actions,
            {
                width: 40,
                header: "Play",
                renderer: function(v, o, r) {
                    var title = r.data['disp_title'];
                    if (r.data['episode'])
                        title += ' / ' + r.data['episode'];
                    return '<a href="play/dvrfile/' + r.id +
                           '?title=' + encodeURIComponent(title) + '">Play</a>';
                }
            }],
        tbar: [downloadButton],
        selected: selected,
        help: function() {
            new tvheadend.help('DVR-Finished Recordings', 'dvr_finished.html');
        }
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_failed = function(panel, index) {

    var actions = tvheadend.dvrRowActions();

    var downloadButton = {
        name: 'download',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: 'Download the selected recording',
                iconCls: 'download',
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
        var r = s.getSelections();
        var b = r.length > 0 && r[0].data.filesize > 0;
        abuttons.download.setDisabled(!b);
    }

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
              'duration,filesize,channelname,creator,' +
              'status,sched_status,url',
        columns: {
            filesize: {
                renderer: tvheadend.filesizeRenderer()
            }
        },
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
		lcol: [
            actions,
            {
                width: 40,
                header: "Play",
                renderer: function(v, o, r) {
                    var title = r.data['disp_title'];
                    if (r.data['episode'])
                        title += ' / ' + r.data['episode'];
                    return '<a href="play/dvrfile/' + r.id +
                           '?title=' + encodeURIComponent(title) + '">Play</a>';
                }
            }],
        tbar: [downloadButton],
        selected: selected,
        help: function() {
            new tvheadend.help('DVR-Failed Recordings', 'dvr_failed.html');
        }
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
        iconCls: 'dvrprofiles',
        tabIndex: index,
        add: {
            url: 'api/dvr/config',
            create: { }
        },
        del: true,
        help: function() {
            new tvheadend.help('DVR', 'config_dvr.html');
        }
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
        iconCls: 'autoRec',
        tabIndex: index,
        columns: {
            enabled:      { width: 50 },
            name:         { width: 200 },
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
            comment:      { width: 200 }
        },
        add: {
            url: 'api/dvr/autorec',
            params: {
               list: 'enabled,name,title,channel,tag,content_type,minduration,' +
                     'maxduration,weekdays,start,pri,config_name,comment'
            },
            create: { }
        },
        del: true,
        list: 'enabled,name,title,channel,tag,content_type,minduration,' +
              'maxduration,weekdays,start,pri,config_name,creator,comment',
        columns: {
            weekdays: {
                renderer: function(st) { return tvheadend.weekdaysRenderer(st); }
            }
        },
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('DVR', 'dvr_autorec.html');
        }
    });

    return panel;

};

/**
 *
 */
tvheadend.timerec_editor = function(panel, index) {

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/timerec',
        titleS: 'Time Schedule',
        titleP: 'Time Schedules',
        iconCls: 'time_schedules',
        tabIndex: index,
        columns: {
            enabled:      { width: 50 },
            name:         { width: 200 },
            title:        { width: 300 },
            channel:      { width: 200 },
            weekdays:     { width: 160 },
            start:        { width: 100 },
            stop:         { width: 100 },
            pri:          { width: 80 },
            config_name:  { width: 120 },
            creator:      { width: 200 },
            comment:      { width: 200 }
        },
        add: {
            url: 'api/dvr/timerec',
            params: {
               list: 'enabled,name,title,channel,weekdays,start,stop,pri,config_name,comment'
            },
            create: { }
        },
        del: true,
        list: 'enabled,name,title,channel,weekdays,start,stop,pri,config_name,comment',
        columns: {
            weekdays: {
                renderer: function(st) { return tvheadend.weekdaysRenderer(st); }
            }
        },
        sort: {
          field: 'name',
          direction: 'ASC'
        },
        help: function() {
            new tvheadend.help('DVR', 'dvr_timerec.html');
        }
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
        iconCls: 'dvr',
        items: []
    });
    tvheadend.dvr_upcoming(p, 0);
    tvheadend.dvr_finished(p, 1);
    tvheadend.dvr_failed(p, 2);
    tvheadend.autorec_editor(p, 3);
    tvheadend.timerec_editor(p, 4);
    return p;
}
