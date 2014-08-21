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
        ]
    });
}

/**
 *
 */
tvheadend.dvr_upcoming = function(panel, index) {

    var actions = tvheadend.dvrRowActions();

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_upcoming',
        comet: 'dvrentry',
        titleS: 'Upcoming Recording',
        titleP: 'Upcoming Recordings',
        iconCls: 'clock',
        tabIndex: index,
        add: {
            url: 'api/dvr/entry',
            params: {
               list: 'disp_title,start,start_extra,stop,stop_extra,' +
                     'channel,config_name',
            },
            create: { }
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

    tvheadend.idnode_grid(panel, {
        url: 'api/dvr/entry',
        gridURL: 'api/dvr/entry/grid_finished',
        readonly: true,
        comet: 'dvrentry',
        titleS: 'Finished Recording',
        titleP: 'Finished Recordings',
        iconCls: 'television',
        tabIndex: index,
        del: true,
        list: 'disp_title,episode,start_real,stop_real,' +
              'duration,filesize,channelname,creator,' +
              'sched_status',
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
        readonly: true,
        comet: 'dvrentry',
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
        comet: 'dvrautorec',
        titleS: 'DVR AutoRec Entry',
        titleP: 'DVR AutoRec Entries',
        iconCls: 'wand',
        tabIndex: index,
        add: {
            url: 'api/dvr/autorec',
            params: {
               list: 'enable,title,channel,tag,content_type,minduration,' +
                     'weekdays,approx_time,pri,config_name,comment',
            },
            create: { }
        },
        del: true,
        list: 'enable,title,channel,tag,content_type,minduration,' +
              'weekdays,approx_time,pri,config_name,creator,comment',
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
tvheadend.dvr = function() {
    var panel = new Ext.TabPanel({
        activeTab: 0,
        autoScroll: true,
        title: 'Digital Video Recorder',
        iconCls: 'drive',
        items: [],
    });
    tvheadend.dvr_upcoming(panel, 0);
    tvheadend.dvr_finished(panel, 1);
    tvheadend.dvr_failed(panel, 2);
    tvheadend.autorec_editor(panel, 3);
    return panel;
}
