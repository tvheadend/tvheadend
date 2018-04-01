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
        var summary = params[3].value;
        var episode = params[4].value;
        var start_real = params[5].value;
        var stop_real = params[6].value;
        var duration = params[7].value;
        var desc = params[8].value;
        var status = params[9].value;
        var filesize = params[10].value;
        var comment = params[11].value;
        var duplicate = params[12].value;
        var autorec_caption = params[13].value;
        var timerec_caption = params[14].value;
        var image = params[15].value;
        var copyright_year = params[16].value;
        var credits = params[17].value;
        var keyword = params[18].value;
        var category = params[19].value;
        var first_aired = params[20].value;
        var genre = params[21].value;
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

        var icons = tvheadend.getContentTypeIcons({"category" : category, "genre" : genre}, "x-dialog-category-large-icon");
        if (icons)
            content += '<div class="x-epg-icons">' + icons + '</div>';
        var displayTitle = title;
        if (copyright_year)
            displayTitle += "&nbsp;(" + copyright_year + ")";
        if (title)
            content += '<div class="x-epg-title">' + displayTitle + '</div>';
        if (subtitle && (!desc || (desc && subtitle != desc)))
            content += '<div class="x-epg-title">' + subtitle + '</div>';
        if (episode)
            content += '<div class="x-epg-title">' + episode + '</div>';
        if (start_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Start Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(start_real * 1000) + '</span></div>';
        if (stop_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Stop Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(stop_real * 1000) + '</span></div>';
        /* We have to *1000 here (and not in epg.js) since Date requires ms and epgStore has it already converted */
        if (first_aired)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('First Aired') + ':</span><span class="x-epg-body">' + tvheadend.niceDateYearMonth(first_aired * 1000, start_real * 1000) + '</span></div>';
        if (duration)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Duration') + ':</span><span class="x-epg-body">' + parseInt(duration / 60) + ' ' + _('min') + '</span></div>';
        if (chicon) {
            content += '</div>'; /* x-epg-left */
            content += '<div class="x-epg-bottom">';
        }
        if (image != null && image.length > 0) {
          content += '<img class="x-epg-image" src="' + image + '">';
        }

        content += '<hr class="x-epg-hr"/>';
        if (summary && (!subtitle || subtitle != summary))
            content += '<div class="x-epg-summary">' + summary + '</div>';
        if (desc) {
            content += '<div class="x-epg-desc">' + desc + '</div>';
            content += '<hr class="x-epg-hr"/>';
        }
        content += tvheadend.getDisplayCredits(credits);
        if (keyword)
          content += tvheadend.sortAndAddArray(keyword, _('Keywords'));
        if (category)
          content += tvheadend.sortAndAddArray(category, _('Categories'));
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
            list: 'channel_icon,disp_title,disp_subtitle,disp_summary,episode_disp,start_real,stop_real,' +
                  'duration,disp_description,status,filesize,comment,duplicate,' +
                  'autorec_caption,timerec_caption,image,copyright_year,credits,keyword,category,' +
                  'first_aired,genre',
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

tvheadend.dvrChannelRenderer = function(st) {
    return function(st) {
        return function(v, meta, rec) {
            if (v) {
                var r = st.getById(v);
                if (r) v = r.data.val;
            }
            if (!v)
                v = rec.data['channelname'];
            return v;
        }
    }
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

tvheadend.displayDuplicate = function(v, meta, rec) {
    if (v == null)
        return '';
    var is_dup = rec.data['duplicate'];
    if (is_dup)
        return "<span class='x-epg-duplicate'>" + v + "</span>";
    else
        return v;
}

/** Render an entry differently if it is a duplicate */
tvheadend.displayWithDuplicateRenderer = function(v, meta, rec) {
    return function() {
        return function(v, meta, rec) {
            return tvheadend.displayDuplicate(v, meta, rec);
        }
    }
}

tvheadend.displayWithYearAndDuplicateRenderer = function(v, meta, rec) {
    return function() {
        return function(v, meta, rec) {
            var v = tvheadend.getDisplayTitle(v, rec);
            return tvheadend.displayDuplicate(v, meta, rec);
        }
    }
}

tvheadend.displayWithYearRenderer = function(v, meta, rec) {
    return function() {
        return function(v, meta, rec) {
            return tvheadend.getDisplayTitle(v, rec);
        }
    }
}

tvheadend.dvrButtonFcn = function(store, select, _url, q) {
    var r = select.getSelections();
    if (r && r.length > 0) {
        var uuids = [];
        for (var i = 0; i < r.length; i++)
            uuids.push(r[i].id);
        var c = {
            url: _url,
            params: {
                uuid: Ext.encode(uuids)
            },
            success: function(d) {
                store.reload();
            }
        };
        if (q) {
            c.question = q;
            tvheadend.AjaxConfirm(c);
        } else {
            tvheadend.Ajax(c);
        }
    }
}

/**
 *
 */
tvheadend.dvr_upcoming = function(panel, index) {

    var actions = tvheadend.dvrRowActions();
    var list = 'disp_title,disp_extratext,channel,start,start_extra,stop,stop_extra,pri,config_name,comment';
    var elist = 'enabled,' +
                (tvheadend.accessUpdate.admin ?
                list + ',episode_disp,owner,creator' : list) + ',retention,removal';
    var duplicates = 0;
    var buttonFcn = tvheadend.dvrButtonFcn;
    var columnId = null;

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
            buttonFcn(store, select, 'api/dvr/entry/stop',
                      _('Do you really want to gracefully stop/unschedule the selection?'));
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
            buttonFcn(store, select, 'api/dvr/entry/cancel',
                      _('Do you really want to abort/unschedule the selection?'));
        }
    };

    var prevrecButton = {
        name: 'prevrec',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Toggle the previously recorded state.'),
                iconCls: 'prevrec',
                text: _('Previously recorded'),
                disabled: true
            });
        },
        callback: function(conf, e, store, select) {
            buttonFcn(store, select, 'api/dvr/entry/prevrec/toggle',
                      _('Do you really want to toggle the previously recorded state for the selected recordings?'));
        }
    };

    function updateDupText(button, dup) {
        button.setText(dup ? _('Hide duplicates') : _('Show duplicates'));
    }

    var dupButton = {
        name: 'dup',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Toggle the view of the duplicate DVR entries.'),
                iconCls: 'duprec',
                text: _('Show duplicates')
            });
        },
        callback: function(conf, e, store, select) {
            duplicates ^= 1;
            select.grid.colModel.setHidden(columnId, !duplicates);
            select.grid.bottomToolbar.changePage(0);
            updateDupText(this, duplicates);
            store.baseParams.duplicates = duplicates;
            store.reload();
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
        abuttons.prevrec.setDisabled(recording >= 1);
    }

    function beforeedit(e, grid) {
        if (e.record.data.sched_status == 'recording')
            return false;
    }

    function viewready(grid) {
        var d = grid.store.baseParams.duplicates;
        updateDupText(grid.abuttons['dup'], d);
        if (!d) {
            columnId = grid.colModel.findColumnIndex('duplicate');
            grid.colModel.setHidden(columnId, true);
        }
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_upcoming',
        titleS: _('Upcoming Recording'),
        titleP: _('Upcoming / Current Recordings'),
        iconCls: 'upcomingRec',
        tabIndex: index,
        extraParams: function(params) {
            params.duplicates = duplicates
        },
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
        list: 'category,enabled,duplicate,disp_title,disp_extratext,episode_disp,' +
              'channel,image,copyright_year,start_real,stop_real,duration,pri,filesize,' +
              'sched_status,errors,data_errors,config_name,owner,creator,comment,genre',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearAndDuplicateRenderer()
            },
            disp_extratext: {
                renderer: tvheadend.displayWithDuplicateRenderer()
            },
            filesize: {
                renderer: tvheadend.filesizeRenderer()
            },
            genre: {
                renderer: function(vals, meta, record) {
                    return function(vals, meta, record) {
                      var r = [];
                      Ext.each(vals, function(v) {
                        v = tvheadend.contentGroupFullLookupName(v);
                        if (v)
                          r.push(v);
                      });
                      if (r.length < 1) return "";
                      return r.join(',');
                  }
                }
            }
        },
        sort: {
          field: 'start_real',
          direction: 'ASC'
        },
        plugins: [actions],
        lcol: [
            actions,
            tvheadend.contentTypeAction,
        ],
        tbar: [stopButton, abortButton, prevrecButton, dupButton],
        selected: selected,
        beforeedit: beforeedit,
        viewready: viewready
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_finished = function(panel, index) {

    var actions = tvheadend.dvrRowActions();
    var buttonFcn = tvheadend.dvrButtonFcn;
    var pageSize = 50;
    var activePage = 0;

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
            buttonFcn(store, select, 'api/dvr/entry/rerecord/toggle');
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
            buttonFcn(store, select, 'api/dvr/entry/move/failed');
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
            buttonFcn(store, select, 'api/dvr/entry/remove',
                      _('Do you really want to remove the selected recordings from storage?'));
        }
    };

    function groupingText(field) {
        return field ? _('Enable grouping') : _('Disable grouping');
    }

    var groupingButton = {
        name: 'grouping',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('When enabled, group the recordings by the selected column.'),
                iconCls: 'grouping',
                text: _('Enable grouping')
            });
        },
        callback: function(conf, e, store, select) {
            this.setText(groupingText(store.groupField));
            if (!store.groupField){
                select.grid.view.enableGrouping = true;
                pageSize = select.grid.bottomToolbar.pageSize; // Store page size
                activePage = select.grid.bottomToolbar.getPageData().activePage; // Store active page
                select.grid.bottomToolbar.pageSize = 999999999 // Select all rows
                select.grid.bottomToolbar.changePage(0);
                store.reload();
                select.grid.store.groupBy(store.sortInfo.field);
                select.grid.fireEvent('groupchange', select.grid, store.getGroupState());
                select.grid.view.refresh();
            }else{
                select.grid.bottomToolbar.pageSize = pageSize // Restore page size
                select.grid.bottomToolbar.changePage(activePage); // Restore previous active page
                store.reload();
                store.clearGrouping();
                select.grid.view.enableGrouping = false;
                select.grid.fireEvent('groupchange', select.grid, null);
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

    function viewready(grid) {
        grid.abuttons['grouping'].setText(groupingText(!grid.store.groupField));
    }

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_finished',
        readonly: true,
        titleS: _('Finished Recording'),
        titleP: _('Finished Recordings'),
        iconCls: 'finishedRec',
        tabIndex: index,
        edit: {
            params: {
                list: tvheadend.admin ? "disp_title,disp_extratext,episode_disp,playcount,retention,removal,owner,comment" :
                                        "retention,removal,comment"
            }
        },
        del: false,
        list: 'disp_title,disp_extratext,episode_disp,channel,channelname,' +
              'start_real,stop_real,duration,filesize,copyright_year,' +
              'sched_status,errors,data_errors,playcount,url,config_name,owner,creator,comment,',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
            },
            channel: {
                renderer: tvheadend.dvrChannelRenderer(),
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
        lcol: [
            actions,
            {
                width: 40,
                header: _("Play"),
                tooltip: _("Play"),
                renderer: function(v, o, r) {
                    var title = r.data['disp_title'];
                    if (r.data['episode_disp'])
                        title += ' / ' + r.data['episode_disp'];
                    return tvheadend.playLink('play/dvrfile/' + r.id, title);
                }
            }],
        tbar: [removeButton, downloadButton, rerecordButton, moveButton, groupingButton],
        selected: selected,
        viewready: viewready,
        viewTpl: '{text} ({[values.rs.length]} {[values.rs.length > 1 ? "' +
                  _('Recordings') + '" : "' + _('Recording') + '"]})'
    });

    return panel;
};

/**
 *
 */
tvheadend.dvr_failed = function(panel, index) {

    var actions = tvheadend.dvrRowActions();
    var buttonFcn = tvheadend.dvrButtonFcn;

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
            buttonFcn(store, select, 'api/dvr/entry/rerecord/toggle');
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
            buttonFcn(store, select, 'api/dvr/entry/move/finished');
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
        list: 'disp_title,disp_extratext,episode_disp,channel,channelname,' +
              'image,copyright_year,start_real,stop_real,duration,filesize,status,' +
              'sched_status,errors,data_errors,playcount,url,config_name,owner,creator,comment',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
            },
            channel: {
                renderer: tvheadend.dvrChannelRenderer(),
            },
            filesize: {
                renderer: tvheadend.filesizeRenderer()
            }
        },
        sort: {
          field: 'start_real',
          direction: 'DESC'
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
                    if (r.data['episode_disp'])
                        title += ' / ' + r.data['episode_disp'];
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
    var buttonFcn = tvheadend.dvrButtonFcn;

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
            buttonFcn(store, select, 'api/dvr/entry/rerecord/toggle');
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
        edit: { params: { list: tvheadend.admin ? "retention,owner,disp_title,disp_extratext,episode_disp,comment" : "retention,comment" } },
        del: true,
        list: 'disp_title,disp_extratext,episode_disp,channel,channelname,image,' +
              'copyright_year,start_real,stop_real,duration,status,' +
              'sched_status,errors,data_errors,url,config_name,owner,creator,comment',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
            },
            channel: {
                renderer: tvheadend.dvrChannelRenderer(),
            }
        },
        sort: {
          field: 'start_real',
          direction: 'DESC'
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
               'record,tag,btype,content_type,cat1,cat2,cat3,minduration,maxduration,' +
               'star_rating,dedup,directory,config_name,comment,pri';
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
            cat1:         { width: 300 },
            cat2:         { width: 300 },
            cat3:         { width: 300 },
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
            star_rating:  { width: 80 },
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
              'weekdays,minduration,maxduration,record,btype,content_type,cat1,cat2,cat3' +
              'star_rating,pri,dedup,directory,config_name,owner,creator,comment',
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
