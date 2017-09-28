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
        var subtitle = params[2].value;
        var episode = params[3].value;
        var start_real = params[4].value;
        var stop_real = params[5].value;
        var duration = params[6].value;
        var desc = params[7].value;
        var status = params[8].value;
        var filesize = params[9].value;
        var comment = params[10].value;
        var duplicate = params[11].value;
        var autorec_caption = params[12].value;
        var timerec_caption = params[13].value;
        var content = '';
        var but;

        if (chicon != null && chicon.length > 0) {
            content += '<img class="x-epg-chicon" src="' + chicon + '">';
        } else {
            chicon = null;
        }

        if (chicon)
            content += '<div class="x-epg-left">';

        if (duplicate)
            content += '<div class="x-epg-meta"><font color="red"><span class="x-epg-prefix">' + _('Will be skipped') + '<br>' + _('because it is a rerun of:') + '</span>' + tvheadend.niceDate(duplicate * 1000) + '</font></div>';

        if (title)
            content += '<div class="x-epg-title">' + title + '</div>';
        if (subtitle)
            content += '<div class="x-epg-title">' + subtitle + '</div>';
        if (episode)
            content += '<div class="x-epg-title">' + episode + '</div>';
        if (start_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Start Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(start_real * 1000) + '</span></div>';
        if (stop_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Stop Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(stop_real * 1000) + '</span></div>';
        if (duration)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Duration') + ':</span><span class="x-epg-body">' + parseInt(duration / 60) + ' ' + _('min') + '</span></div>';
        if (chicon) {
            content += '</div>'; /* x-epg-left */
            content += '<div class="x-epg-bottom">';
        }
        content += '<hr class="x-epg-hr"/>';
        if (desc) {
            content += '<div class="x-epg-desc">' + desc + '</div>';
            content += '<hr class="x-epg-hr"/>';
        }
        if (status)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Status') + ':</span><span class="x-epg-body">' + status + '</span></div>';
        if (filesize)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('File size') + ':</span><span class="x-epg-body">' + parseInt(filesize / 1000000) + ' MB</span></div>';
        if (comment)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Comment') + ':</span><span class="x-epg-body">' + comment + '</span></div>';
        if (autorec_caption)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Autorec') + ':</span><span class="x-epg-body">' + autorec_caption + '</span></div>';
        if (timerec_caption)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Time Scheduler') + ':</span><span class="x-epg-body">' + timerec_caption + '</span></div>';
        if (chicon)
            content += '</div>'; /* x-epg-bottom */

        var buttons = [];

        buttons.push(new Ext.Button({
            handler: searchIMDB,
            iconCls: 'imdb',
            tooltip: _('Search IMDB (for title)'),
        }));

        buttons.push(new Ext.Button({
            handler: searchTheTVDB,
            iconCls: 'thetvdb',
            tooltip: _('Search TheTVDB (for title)'),
        }));

        function searchIMDB() {
            window.open('http://akas.imdb.com/find?q=' +
                        encodeURIComponent(title), '_blank');
        }

        function searchTheTVDB(){
            window.open('http://thetvdb.com/?string='+
                        encodeURIComponent(title)+'&searchseriesid=&tab=listseries&function=Search','_blank');
        }

        var win = new Ext.Window({
            title: title,
            iconCls: 'info',
            layout: 'fit',
            width: 650,
            height: 450,
            constrainHeader: true,
            buttonAlign: 'center',
            buttons: buttons,
            html: content
        });

        win.show();
    }

    tvheadend.loading(1);
    Ext.Ajax.request({
        url: 'api/idnode/load',
        params: {
            uuid: uuid,
            list: 'channel_icon,disp_title,disp_subtitle,episode,start_real,stop_real,' +
                  'duration,disp_description,status,filesize,comment,duplicate,' +
                  'autorec_caption,timerec_caption'
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
        header: _('Details'),
        tooltip: _('Details'),
        width: 45,
        actions: [
            {
                iconIndex: 'sched_status'
            },
            {
                iconCls: 'info',
                qtip: _('Recording details'),
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
            v = _("Every day");
        } else if (d.length == 0) {
            v = _("No days");
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

/** Render an entry differently if it is a duplicate */
tvheadend.displayWithDuplicateRenderer = function(value, meta, record) {
    return function() {
        return function(value, meta, record) {
            if (value == null)
                return '';
            var is_dup = record.data['duplicate'];
            if (is_dup)
                return "<span class='x-epg-duplicate'>" + value + "</span>";
            else
                return value;
        }
    }
}

/**
 *
 */
tvheadend.dvr_upcoming = function(panel, index) {

    var actions = tvheadend.dvrRowActions();
    var list = 'disp_title,channel,start,start_extra,stop,stop_extra,pri,config_name,comment';
    var elist = 'enabled,' +
                (tvheadend.accessUpdate.admin ?
                list + ',owner,creator' : list) + ',retention,removal';

    var stopButton = {
        name: 'stop',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Stop the selected recording'),
                iconCls: 'stopRec',
                text: _('Stop'),
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
                    url: 'api/dvr/entry/stop',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    },
                    question: _('Do you really want to gracefully stop/unschedule the selection?')
                });
            }
        }
    };

    var abortButton = {
        name: 'abort',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Abort the selected recording'),
                iconCls: 'abort',
                text: _('Abort'),
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
                    question: _('Do you really want to abort/unschedule the selection?')
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
        abuttons.stop.setDisabled(recording < 1);
        abuttons.abort.setDisabled(recording < 1);
    }

    function beforeedit(e, grid) {
        if (e.record.data.sched_status == 'recording')
            return false;
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_upcoming',
        titleS: _('Upcoming Recording'),
        titleP: _('Upcoming / Current Recordings'),
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
                list: elist
            }
        },
        del: true,
        list: 'enabled,duplicate,disp_title,disp_subtitle,episode,channel,' +
              'start_real,stop_real,duration,pri,filesize,' +
              'sched_status,errors,data_errors,config_name,owner,creator,comment',
        columns: {
            disp_title: {
              renderer: tvheadend.displayWithDuplicateRenderer()
            },
            disp_subtitle: {
              renderer: tvheadend.displayWithDuplicateRenderer()
            },
            filesize: {
                renderer: tvheadend.filesizeRenderer()
            }
        },
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
        lcol: [actions],
        tbar: [stopButton, abortButton],
        selected: selected,
        beforeedit: beforeedit
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
                tooltip: _('Download the selected recording'),
                iconCls: 'download',
                text: _('Download'),
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

    var rerecordButton = {
        name: 'rerecord',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Toggle re-record functionality'),
                iconCls: 'rerecord',
                text: _('Re-record'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/dvr/entry/rerecord/toggle',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };

    var moveButton = {
        name: 'move',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Mark the selected recording as failed'),
                iconCls: 'movetofailed',
                text: _('Move to failed'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/dvr/entry/move/failed',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };
    
    var removeButton = {
        name: 'remove',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Remove the selected recording from storage'),
                iconCls: 'remove',
                text: _('Remove'),
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
                    url: 'api/dvr/entry/remove',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    },
                    question: _('Do you really want to remove the selected recordings from storage?')   
                });
            }
        }
    };

    function selected(s, abuttons) {
        var r = s.getSelections();
        var b = r.length > 0 && r[0].data.filesize > 0;
        abuttons.download.setDisabled(!b);
        abuttons.rerecord.setDisabled(!b);
        abuttons.move.setDisabled(!b);
        abuttons.remove.setDisabled(!b);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_finished',
        readonly: true,
        titleS: _('Finished Recording'),
        titleP: _('Finished Recordings'),
        iconCls: 'finishedRec',
        tabIndex: index,
        edit: { params: { list: tvheadend.admin ? "playcount,retention,removal,owner,comment" : "retention,removal,comment" } },
        del: false,
        list: 'disp_title,disp_subtitle,episode,channelname,' +
              'start_real,stop_real,duration,filesize,' +
              'sched_status,errors,data_errors,playcount,url,config_name,owner,creator,comment',
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
                header: _("Play"),
                tooltip: _("Play"),
                renderer: function(v, o, r) {
                    var title = r.data['disp_title'];
                    if (r.data['episode'])
                        title += ' / ' + r.data['episode'];
                    return tvheadend.playLink('play/dvrfile/' + r.id, title);
                }
            }],
        tbar: [removeButton, downloadButton, rerecordButton, moveButton],
        selected: selected
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
                tooltip: _('Download the selected recording'),
                iconCls: 'download',
                text: _('Download'),
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

    var rerecordButton = {
        name: 'rerecord',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Toggle re-record functionality'),
                iconCls: 'rerecord',
                text: _('Re-record'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/dvr/entry/rerecord/toggle',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };

    var moveButton = {
        name: 'move',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Mark the selected recording as finished'),
                iconCls: 'movetofinished',
                text: _('Move to finished'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/dvr/entry/move/finished',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };

    function selected(s, abuttons) {
        var r = s.getSelections();
        var b = r.length > 0 && r[0].data.filesize > 0;
        abuttons.download.setDisabled(!b);
        abuttons.rerecord.setDisabled(r.length <= 0);
        abuttons.move.setDisabled(r.length <= 0);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_failed',
        readonly: true,
        titleS: _('Failed Recording'),
        titleP: _('Failed Recordings'),
        iconCls: 'exclamation',
        tabIndex: index,
        edit: { params: { list: tvheadend.admin ? "playcount,retention,removal,owner,comment" : "retention,removal,comment" } },
        del: true,
        delquestion: _('Do you really want to delete the selected recordings?') + '<br/><br/>' +
                     _('The associated file will be removed from storage.'),
        list: 'disp_title,disp_subtitle,episode,channelname,' +
              'start_real,stop_real,duration,filesize,status,' +
              'sched_status,errors,data_errors,playcount,url,config_name,owner,creator,comment',
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
                header: _("Play"),
                tooltip: _("Play"),
                renderer: function(v, o, r) {
                    var title = r.data['disp_title'];
                    if (r.data['episode'])
                        title += ' / ' + r.data['episode'];
                    return tvheadend.playLink('play/dvrfile/' + r.id, title);
                }
            }],
        tbar: [downloadButton, rerecordButton, moveButton],
        selected: selected
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_removed = function(panel, index) {

    var actions = tvheadend.dvrRowActions();

    var rerecordButton = {
        name: 'rerecord',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Toggle re-record functionality'),
                iconCls: 'rerecord',
                text: _('Re-record'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            var r = select.getSelections();
            if (r && r.length > 0) {
                var uuids = [];
                for (var i = 0; i < r.length; i++)
                    uuids.push(r[i].id);
                tvheadend.Ajax({
                    url: 'api/dvr/entry/rerecord/toggle',
                    params: {
                        uuid: Ext.encode(uuids)
                    },
                    success: function(d) {
                        store.reload();
                    }
                });
            }
        }
    };
    
    function selected(s, abuttons) {
        var r = s.getSelections();
        abuttons.rerecord.setDisabled(r.length <= 0);
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_removed',
        readonly: true,
        titleS: _('Removed Recording'),
        titleP: _('Removed Recordings'),
        iconCls: 'remove',
        tabIndex: index,
        uilevel: 'expert',
        edit: { params: { list: tvheadend.admin ? "retention,owner,comment" : "retention,comment" } },
        del: true,
        list: 'disp_title,disp_subtitle,episode,channelname,' +
              'start_real,stop_real,duration,status,' +
              'sched_status,errors,data_errors,url,config_name,owner,creator,comment',
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
        lcol: [actions],
        tbar: [rerecordButton],
        selected: selected
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
        titleS: _('Digital Video Recorder Profile'),
        titleP: _('Digital Video Recorder Profiles'),
        titleC: _('Profile Name'),
        iconCls: 'dvrprofiles',
        tabIndex: index,
        add: {
            url: 'api/dvr/config',
            create: { }
        },
        del: true
    });

    return panel;

}

/**
 *
 */
tvheadend.autorec_editor = function(panel, index) {

    var list = 'name,title,fulltext,channel,start,start_window,weekdays,' +
               'record,tag,btype,content_type,minduration,maxduration,' +
               'dedup,directory,config_name,comment';
    var elist = 'enabled,start_extra,stop_extra,' +
                (tvheadend.accessUpdate.admin ?
                list + ',owner,creator' : list) + ',pri,retention,removal,maxcount,maxsched';

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/autorec',
        titleS: _('Autorec'),
        titleP: _('Autorecs'),
        iconCls: 'autoRec',
        tabIndex: index,
        columns: {
            enabled:      { width: 50 },
            name:         { width: 200 },
            directory:    { width: 200 },
            title:        { width: 300 },
            fulltext:     { width: 70 },
            channel:      { width: 200 },
            tag:          { width: 200 },
            btype:        { width: 50 },
            content_type: { width: 100 },
            minduration:  { width: 100 },
            maxduration:  { width: 100 },
            weekdays:     { width: 160 },
            start:        { width: 80 },
            start_window: { width: 80 },
            start_extra:  { width: 80 },
            stop_extra:   { width: 80 },
            weekdays: {
                width: 120,
                renderer: function(st) { return tvheadend.weekdaysRenderer(st); }
            },
            pri:          { width: 80 },
            dedup:        { width: 160 },
            retention:    { width: 80 },
            removal:      { width: 80 },
            maxcount:     { width: 80 },
            maxsched:     { width: 80 },
            config_name:  { width: 120 },
            owner:        { width: 100 },
            creator:      { width: 200 },
            comment:      { width: 200 }
        },
        add: {
            url: 'api/dvr/autorec',
            params: {
               list: list
            },
            create: { }
        },
        edit: {
            params: {
                list: elist
            },
        },
        del: true,
        list: 'enabled,name,title,fulltext,channel,tag,start,start_window,' +
              'weekdays,minduration,maxduration,btype,content_type,' +
              'pri,dedup,directory,config_name,owner,creator,comment',
        sort: {
          field: 'name',
          direction: 'ASC'
        }
    });

    return panel;

};

/**
 *
 */
tvheadend.timerec_editor = function(panel, index) {

    var list = 'name,title,channel,start,stop,weekdays,' +
               'directory,config_name,comment';
    var elist = 'enabled,' +
                (tvheadend.accessUpdate.admin ?
                list + ',owner,creator' : list) + ',pri,retention,removal';

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/timerec',
        titleS: _('Timer'),
        titleP: _('Timers'),
        iconCls: 'timers',
        tabIndex: index,
        columns: {
            enabled:      { width: 50 },
            name:         { width: 200 },
            directory:    { width: 200 },
            title:        { width: 300 },
            channel:      { width: 200 },
            weekdays: {
                width: 120,
                renderer: function(st) { return tvheadend.weekdaysRenderer(st); }
            },
            start:        { width: 120 },
            stop:         { width: 120 },
            pri:          { width: 80 },
            retention:    { width: 80 },
            removal:      { width: 80 },
            config_name:  { width: 120 },
            owner:        { width: 100 },
            creator:      { width: 200 },
            comment:      { width: 200 }
        },
        add: {
            url: 'api/dvr/timerec',
            params: {
               list: list
            },
            create: { }
        },
        edit: {
            params: {
                list: elist
            },
        },
        del: true,
        list: 'enabled,name,title,channel,start,stop,weekdays,pri,directory,config_name,owner,creator,comment',
        sort: {
          field: 'name',
          direction: 'ASC'
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
        title: _('Digital Video Recorder'),
        iconCls: 'dvr',
        items: []
    });
    tvheadend.dvr_upcoming(p, 0);
    tvheadend.dvr_finished(p, 1);
    tvheadend.dvr_failed(p, 2);
    tvheadend.dvr_removed(p, 3);
    tvheadend.autorec_editor(p, 4);
    tvheadend.timerec_editor(p, 5);
    return p;
}
