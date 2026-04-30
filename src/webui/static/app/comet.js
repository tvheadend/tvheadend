/**
 * Comet interfaces
 */
tvheadend.Comet = Ext.extend(Ext.util.Observable, {});

tvheadend.comet = new tvheadend.Comet();
tvheadend.ws = null;
tvheadend.wsURI = null;
tvheadend.boxid = null;

tvheadend.cometError = function() {
    tvheadend.log(_('There seems to be a problem with the '
            + 'live update feed from Tvheadend. '
            + 'Trying to reconnect...'),
            'font-weight: bold; color: #f00');
};

tvheadend.cometReconnecting = function() {
    tvheadend.log(_('Reconnecting to Tvheadend...'),
                    'font-weight: bold; color: #fa0');
};

tvheadend.cometReconnected = function() {
    tvheadend.log(_('Reconnected to Tvheadend'),
                    'font-weight: bold; color: #080');
};

tvheadend.cometParse = function(responsetxt) {
    try {
        var response = Ext.util.JSON.decode(responsetxt);
        if ('boxid' in response) {
            if (tvheadend.boxid && tvheadend.boxid !== response.boxid)
                window.location.reload();
            tvheadend.boxid = response.boxid;
        }
        if (Array.isArray(response.messages)) {
            for (var x = 0; x < response.messages.length; x++) {
                var m = response.messages[x];
                //if (0) console.log(JSON.stringify(m), null, " ");
                try {
                    tvheadend.comet.fireEvent(m.notificationClass, m);
                } catch (e) {
                    tvheadend.log(_('Comet failure') + ' [e=' + e.message + ']');
                }
            }
        }
    } catch (e) {
        tvheadend.log(_('Invalid JSON from comet') + ' [e=' + e.message + ']');
    }
};

tvheadend.cometPoller = function() {
    var failures = 0;
    var cometRequest = new Ext.util.DelayedTask(function() {
        Ext.Ajax.request({
            url: 'comet/poll',
            params: {
                boxid: (tvheadend.boxid ? tvheadend.boxid : null),
                immediate: failures > 0 ? 1 : 0
            },
            success: function(result, request) {
                if (failures > 1)
                    tvheadend.cometReconnected();
                failures = 0;

                tvheadend.cometParse(result.responseText);
                cometRequest.delay(100);
            },
            failure: function(result, request) {
                if (failures === 1)
                    tvheadend.cometError();
                if (failures > 1)
                    tvheadend.cometReconnecting();
                failures++;

                cometRequest.delay(failures ? 1000 : 1);
            }
        });
    });
    cometRequest.delay(100);
};

tvheadend.cometWebsocket = function() {
    var failures = 0;
    var cometRequest = new Ext.util.DelayedTask(function() {
        var uri = tvheadend.wsURI;
        if (tvheadend.boxid)
          uri = uri + '?boxid=' + tvheadend.boxid;
        tvheadend.ws = new WebSocket(uri, ['tvheadend-comet']);
        if (failures > 5)
            window.location.reload();
        if (failures > 1)
            tvheadend.cometReconnecting();
        tvheadend.ws.onmessage = function(ev) {
            if (failures > 1)
                tvheadend.cometReconnected();
            failures = 0;
            tvheadend.cometParse(ev.data);
        };
        tvheadend.ws.onerror = function(ev) {
            if (failures === 1)
                tvheadend.cometError();
            failures++;
        };
        tvheadend.ws.onclose = function(ev) {
            cometRequest.delay(failures ? 1000 : 50);
        };
    });
    cometRequest.delay(100);
};

tvheadend.cometInit = function() {
    var isSafari = /^((?!chrome|android).)*safari/i.test(navigator.userAgent);
    if (!isSafari && ("WebSocket" in window)) {
      var loc = window.location;
      var path = loc.pathname.substring(0, loc.pathname.lastIndexOf("/"));
      tvheadend.wsURI = (loc.protocol === "https:" ? "wss:" : "ws:") + "//" + loc.host + path + "/comet/ws";
      tvheadend.cometWebsocket();
    } else {
      tvheadend.cometPoller();
    }
};
