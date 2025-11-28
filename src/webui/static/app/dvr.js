/*
 * DVR Config / Schedule / Log editor and viewer
 */

/**
 *
 */

tvheadend.labelFormattingParser = function(description) {
    if (tvheadend.label_formatting){
        return description.replace(/\[COLOR\s(.*?)\]/g, '<font style="color:$1">')
	                   .replace(/\[\/COLOR\]/g, '<\/font>')
	                   .replace(/\[B\]/g, '<b>')
	                   .replace(/\[\/B\]/g, '<\/b>')
	                   .replace(/\[I\]/g, '<i>')
	                   .replace(/\[CR\]/g, '<br>')
	                   .replace(/\[\/I\]/g, '<\/i>')
	                   .replace(/\[UPPERCASE\](.*)\[\/UPPERCASE\]/g, function(match, group) {return group.toUpperCase();})
	                   .replace(/\[LOWERCASE\](.*)\[\/LOWERCASE\]/g, function(match, group) {return group.toLowerCase();})
	                   .replace(/\[CAPITALIZE\](.*)\[\/CAPITALIZE\]/g, function(match, group) {return group.split(/\s+/).map(w => w[0].toUpperCase() + w.slice(1)).join(' ');});
    }else return description;
};

tvheadend.dvrDetails = function(grid, index) {
    var current_index = index;
    var win;
    var updateTimer;
    // We need a unique DOM id in case user opens two dialogs.
    var nextButtonId = Ext.id();
    var previousButtonId = Ext.id();
    // Our title is passed to search functions (such as imdb)
    // So always ensure this does not contain channel info.
    function getTitle(d) {
      var params = d[0].params;
      return params[1].value;
    }

    function getDialogTitle(d) {
      var params = d[0].params;
      var fields = [];
      var evTitle = params[1].value;
      if (evTitle && evTitle.length) fields.push(evTitle);
      var evEp = params[4].value;
      if (evEp && evEp.length) fields.push(evEp);
      var channelname = params[22].value;
      if (channelname && channelname.length) fields.push(channelname);
      return fields.join(' - ');
    }

    function getDialogContent(d) {
        var event = {};
        for (var param of d[0].params) {
          if ('value' in param)
            event[param.id] = param.value;
          else if ('default' in param)
            event[param.id] = param.default;
        }
        var content = '<div class="dvr-details-dialog">' +
        '<div class="dvr-details-dialog-background-image"></div>' +
        '<div class="dvr-details-dialog-content">';

        if (event.channel_icon != null && event.channel_icon.length > 0) {
            content += '<img class="x-epg-chicon" src="' + event.channel_icon + '">';
        } else {
            event.channel_icon = null;
        }

        if (event.channel_icon)
            content += '<div class="x-epg-left">';

        if (event.duplicate)
            content += '<div class="x-epg-meta"><font color="red"><span class="x-epg-prefix">' + _('Will be skipped') + '<br>' + _('because it is a rerun of:') + '</span>' + tvheadend.niceDate(event.duplicate * 1000) + '</font></div>';

        var icons = tvheadend.getContentTypeIcons({"category" : event.category, "genre" : event.genre}, "x-dialog-category-large-icon");
        if (icons)
            content += '<div class="x-epg-icons">' + icons + '</div>';
        var displayTitle = event.disp_title;
        if (event.copyright_year)
            displayTitle += "&nbsp;(" + event.copyright_year + ")";
        if (event.disp_title)
            content += '<div class="x-epg-title">' + displayTitle + '</div>';
        if (event.disp_subtitle && (!event.disp_description || (event.disp_description && event.disp_subtitle != event.disp_description)))
            content += '<div class="x-epg-title">' + event.disp_subtitle + '</div>';
        if (event.episode_disp)
            content += '<div class="x-epg-title">' + event.episode_disp + '</div>';
        if (event.start_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Start Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(event.start_real * 1000) + '</span></div>';
        if (event.stop_real)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Scheduled Stop Time') + ':</span><span class="x-epg-body">' + tvheadend.niceDate(event.stop_real * 1000) + '</span></div>';
        /* We have to *1000 here (and not in epg.js) since Date requires ms and epgStore has it already converted */
        if (event.first_aired)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('First Aired') + ':</span><span class="x-epg-body">' + tvheadend.niceDateYearMonth(event.first_aired * 1000, event.start_real * 1000) + '</span></div>';
        if (event.duration)
            content += '<div class="x-epg-time"><span class="x-epg-prefix">' + _('Duration') + ':</span><span class="x-epg-body">' + parseInt(event.duration / 60) + ' ' + _('min') + '</span></div>';
        if (event.channel_icon) {
            content += '</div>'; /* x-epg-left */
            content += '<div class="x-epg-bottom">';
        }
        // If we have no image then use fanart image instead.
        var eventimg = '';
        if (event.image != null && event.image.length > 0)
          eventimg = event.image;
        else if (event.fanart_image != null && event.fanart_image.length > 0)
          eventimg = event.fanart_image;
        if (eventimg)
          content += '<div class="x-epg-image-container"><img class="x-epg-image" src="' + eventimg + '"></div>';

        content += '<hr class="x-epg-hr"/>';
        if (event.disp_summary && (!event.disp_subtitle || event.disp_subtitle != event.disp_summary))
            content += '<div class="x-epg-summary">' + event.disp_summary + '</div>';
        if (event.disp_description) {
            content += '<div class="x-epg-desc">' + tvheadend.labelFormattingParser(event.disp_description) + '</div>';
            content += '<hr class="x-epg-hr"/>';
        }

        content += tvheadend.getDisplayCredits(event.credits);
        if (event.keyword)
          content += tvheadend.sortAndAddArray(event.keyword, _('Keywords'));
        if (event.category)
          content += tvheadend.sortAndAddArray(event.category, _('Categories'));
        if (event.rating_icon)
            content += '<img class="x-epg-rlicon" src="' + event.rating_icon + '">';
        if (event.age_rating)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Age Rating') + ':</span><span class="x-epg-desc">' + event.age_rating + '</span></div>';
        if (event.rating_label)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Parental Rating') + ':</span><span class="x-epg-desc">' + event.rating_label + '</span></div>';
        if (event.status)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Status') + ':</span><span class="x-epg-desc">' + event.status + '</span></div>';
        if (event.filesize)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('File size') + ':</span><span class="x-epg-desc">' + parseInt(event.filesize / 1000000) + ' MB</span></div>';
        if (event.filename && (tvheadend.uiviewlevel ? tvheadend.uiviewlevel : tvheadend.uilevel) !== 'basic')  // Only show for 'advanced' and 'expert' levels.
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('File name') + ':</span><span class="x-epg-desc">' + event.filename + '</span></div>';
        if (event.comment)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Comment') + ':</span><span class="x-epg-desc">' + event.comment + '</span></div>';
        if (event.autorec_caption)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Autorec') + ':</span><span class="x-epg-desc">' + event.autorec_caption + '</span></div>';
        if (event.timerec_caption)
            content += '<div class="x-epg-meta"><span class="x-epg-prefix">' + _('Time Scheduler') + ':</span><span class="x-epg-desc">' + event.timerec_caption + '</span></div>';
        if (event.channel_icon)
            content += '</div>'; /* x-epg-bottom */
      content += '</div>';        //dialog content
      return content
    }

  function getDialogButtons(d) {
        var title = getTitle(d);
        var buttons = [];
        var eventId = d[0].params[24].value;

        var comboGetInfo = new Ext.form.ComboBox({
            store: new Ext.data.ArrayStore({
                data: [
                  [1, 'Query IMDB', 'imdb.png'],
                  [2, 'Query TheTVDB', 'thetvdb.png'],
                  [3, 'Query FilmAffinity', 'filmaffinity.png'],
                  [4, 'Query CSFD', 'csfd.png'],
                ],
                id: 0,
                fields: ['value', 'text', 'url']
            }),
            triggerAction: 'all',
            mode: 'local',
            tpl : '<tpl for=".">' +
                  '<div class="x-combo-list-item" ><img src="../static/icons/{url}">&nbsp;&nbsp;{text}</div>' +
                  '</tpl>',
            emptyText:'Find info from ...',
            valueField: 'value',
            displayField: 'text',
            width: 160,
            forceSelection : true,
            editable: false,
            listeners: {
                select: function(combo, records, index) {
                    tvheadend.seachTitleWeb(combo.getValue(), title);
                }
            },
        });

        if (title)
            buttons.push(comboGetInfo);

        buttons.push(new Ext.Toolbar.Button({
            handler: function() { epgAlternativeShowingsDialog(eventId, true) },
            iconCls: 'duprec',
            tooltip: _('Find alternative showings for the DVR entry.'),
        }));
        buttons.push(new Ext.Toolbar.Button({
            handler: function() { epgAlternativeShowingsDialog(eventId, false) },
            iconCls: 'epgrelated',
            tooltip: _('Find related showings for the DVR entry.'),
        }));

        buttons.push(new Ext.Button({
              id: previousButtonId,
              handler: previousEvent,
              iconCls: 'previous',
              tooltip: _('Go to previous event'),
        }));
        buttons.push(new Ext.Button({
            id: nextButtonId,
            handler: nextEvent,
            iconCls: 'next',
            tooltip: _('Go to next event'),
        }));

    return buttons;
  }                             // getDialogButtons

  function updateDialogFanart(d) {
      var params = d[0].params;
      var image=params[15].value;
      var fanart = params[23].value;

      if (updateTimer)
          clearInterval(updateTimer);

      fanart_div = win.el.child(".dvr-details-dialog-background-image");
      if (fanart != null && fanart.length > 0 && fanart_div) {
          // Set the fanart image. The rest of the css _should_ by in the tv.css,
          // but it seemed to ignore it when we applyStyles.
          // We have to explicitly set width/height otherwise the box was 0px tall.
          fanart_div.applyStyles({
              'background' : 'url(' + fanart + ') center center no-repeat',
              'opacity': 0.15,
              'position': 'absolute',
              'width' : '100%',
              'height': '100%',
              // This causes background image to scale on css3 with aspect ratio, image
              // can overflow, vs. 'contain' which will leave blank space top+bottom to
              // ensure image is fully displayed in the window
              'background-size': 'cover',
              // Image can not be clicked on (so events propagate to buttons).
              'pointer-events': 'none',
          });
      }                        // Have fanart div

      if (image != null && image.length > 0 &&
          fanart != null && fanart.length > 0) {
          // We have image and fanart, so alternate the images every x milliseconds.
          var index = 0;
          updateTimer = setInterval(function() {
              if (win.isDestroyed) {
                  clearInterval(updateTimer);
                  return;
              }
              var img_div = win.el.child(".x-epg-image");
              if (img_div && img_div.dom) {
                  var img= img_div.dom;
                  // The img.src can be changed by browser so it does
                  // not match either fanart or image! So we use a
                  // counter to determine which image should be displayed.
                  if (++index % 2) {
                      img.src = fanart;
                  } else {
                      img.src = image;
                  }
              } else {
                  clearInterval(updateTimer);
              }
          }, 10 * 1000);
      }
  }                             //updateDialogFanart

  function showit(d) {
       var dialogTitle = getDialogTitle(d);
       var content = getDialogContent(d);
       var buttons = getDialogButtons(d);
       var windowHeight = Ext.getBody().getViewSize().height - 150;

       win = new Ext.Window({
            title: dialogTitle,
            iconCls: 'info',
            layout: 'fit',
            width: 790,
            height: windowHeight,
            constrainHeader: true,
            buttonAlign: 'center',
            autoScroll: true,
            buttons: buttons,
            html: content
        });
       win.show();
       updateDialogFanart(d);
       checkButtonAvailability(win.fbar)
  }

  function load(store, index, cb) {
      var uuid = store.getAt(index).id;
      tvheadend.loading(1);
      Ext.Ajax.request({
        url: 'api/idnode/load',
        params: {
            uuid: uuid,
            list: 'channel_icon,disp_title,disp_subtitle,disp_summary,episode_disp,start_real,stop_real,' +
                  'duration,disp_description,status,filesize,comment,duplicate,' +
                  'autorec_caption,timerec_caption,image,copyright_year,credits,keyword,category,' +
                  'first_aired,genre,channelname,fanart_image,broadcast,age_rating,rating_label,rating_icon,filename',
        },
        success: function(d) {
            d = json_decode(d);
            tvheadend.loading(0),
            cb(d);
        },
        failure: function(d) {
            tvheadend.loading(0);
        }
      });
  }                           // load

  function previousEvent() {
      --current_index;
      load(store,current_index,updateit);
  }
  function nextEvent() {
      ++current_index;
      load(store,current_index,updateit);
  }

  function checkButtonAvailability(toolBar){
        // If we're at the end of the store then disable the next
        // or previous button.  (getTotalCount is one-based).
        if (current_index == store.getTotalCount() - 1)
          toolBar.getComponent(nextButtonId).disable();
        if (current_index == 0)
          toolBar.getComponent(previousButtonId).disable();
    }

  function updateit(d) {
        var dialogTitle = getDialogTitle(d);
        var content = getDialogContent(d);
        var buttons = getDialogButtons(d);
        win.removeAll();
        // Can't update buttons at the same time...
        win.update({html: content});
        win.setTitle(dialogTitle);
        // ...so remove the buttons and re-add them.
        var tbar = win.fbar;
        tbar.removeAll();
        Ext.each(buttons, function(btn) {
            tbar.addButton(btn);
        });
        updateDialogFanart(d);
        checkButtonAvailability(tbar);
        // Finally, relayout.
        win.doLayout();
  }

    var store = grid.getStore();
    load(store,index,showit);
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
                    new tvheadend.dvrDetails(grid, row);
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

tvheadend.displayWithoutYearRenderer = function(v, meta, rec) {
    return function() {
        return function(v, meta, rec) {
            return v;
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
    var list = 'disp_title,disp_extratext,channel,start,start_extra,stop,stop_extra,pri,uri,config_name,comment';
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

    function selected(s, abuttons) {
        var recording = 0;
        s.each(function(s) {
            if (s.data.sched_status.indexOf('recording') == 0)
                recording++;
        });
        abuttons.stop.setDisabled(recording < 1);
        abuttons.abort.setDisabled(recording < 1);
        abuttons.prevrec.setDisabled(recording >= 1);
        var r = s.getSelections();
        abuttons.epgalt.setDisabled(r.length <= 0);
        abuttons.epgrelated.setDisabled(r.length <= 0);
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
              'sched_status,errors,data_errors,config_name,owner,creator,comment,genre,broadcast,age_rating,rating_label,filename',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearAndDuplicateRenderer(),
                groupRenderer: tvheadend.displayWithoutYearRenderer(),
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
        tbar: [stopButton, abortButton, prevrecButton, epgShowRelatedButtonConf, epgShowAlternativesButtonConf],
        selected: selected,
        beforeedit: beforeedit,
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
        if (grid.store.groupField){
          grid.bottomToolbar.pageSize = 999999999 // Select all rows
          grid.bottomToolbar.changePage(0);
          grid.store.reload();
        }
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
              'sched_status,errors,data_errors,playcount,uri,url,config_name,owner,creator,comment,age_rating,rating_label,filename',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
                groupRenderer: tvheadend.displayWithoutYearRenderer(),
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
                    return tvheadend.playLink('dvrfile/' + r.id, title);
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
              'sched_status,errors,data_errors,playcount,uri,url,config_name,owner,creator,comment,age_rating,rating_label,filename',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
                groupRenderer: tvheadend.displayWithoutYearRenderer(),
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
                    return tvheadend.playLink('dvrfile/' + r.id, title);
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
              'sched_status,errors,data_errors,uri,config_name,owner,creator,comment,age_rating,rating_label',
        columns: {
            disp_title: {
                renderer: tvheadend.displayWithYearRenderer(),
                groupRenderer: tvheadend.displayWithoutYearRenderer(),
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

    var list = 'name,title,fulltext,mergetext,channel,start,start_window,weekdays,' +
               'record,tag,btype,content_type,cat1,cat2,cat3,minduration,maxduration,minyear,maxyear,minseason,maxseason,' +
               'star_rating,dedup,directory,config_name,comment,pri,serieslink';
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
            mergetext:    { width: 70 },
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
            minyear:      { width: 100 },
            maxyear:      { width: 100 },
            minseason:    { width: 100 },
            maxseason:    { width: 100 },
            owner:        { width: 100 },
            creator:      { width: 200 },
            serieslink:   { width: 100 },
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
        list: 'enabled,name,title,fulltext,mergetext,channel,tag,start,start_window,' +
              'weekdays,minduration,maxduration,record,btype,content_type,cat1,cat2,cat3' +
              'star_rating,pri,dedup,directory,config_name,minseason,maxseason,minyear,maxyear,owner,creator,comment,serieslink',
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
