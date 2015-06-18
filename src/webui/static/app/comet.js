/**
 * Comet interfaces
 */
Ext.extend(tvheadend.Comet = function() {
    this.addEvents({ });
}, Ext.util.Observable);

tvheadend.comet = new tvheadend.Comet();
tvheadend.boxid = null;

tvheadend.cometPoller = function() {

    var failures = 0;

    var cometRequest = new Ext.util.DelayedTask(function() {

        Ext.Ajax
                .request({
                    url: 'comet/poll',
                    params: {
                        boxid: (tvheadend.boxid ? tvheadend.boxid : null),
                        immediate: failures > 0 ? 1 : 0
                    },
                    success: function(result, request) {
                        parse_comet_response(result.responseText);

                        if (failures > 1) {
                            tvheadend.log(_('Reconnected to Tvheadend'),
                                    'font-weight: bold; color: #080');
                        }
                        failures = 0;
                    },
                    failure: function(result, request) {
                        cometRequest.delay(failures ? 1000 : 1);
                        if (failures === 1) {
                            tvheadend.log(_('There seems to be a problem with the '
                                    + 'live update feed from Tvheadend. '
                                    + 'Trying to reconnect...'),
                                    'font-weight: bold; color: #f00');
                        }
                        failures++;
                    }
                });
    });

    function parse_comet_response(responsetxt) {
        response = Ext.util.JSON.decode(responsetxt);
        tvheadend.boxid = response.boxid;
        for (x = 0; x < response.messages.length; x++) {
            m = response.messages[x];
            try {
                tvheadend.comet.fireEvent(m.notificationClass, m);
            } catch (e) {
                tvheadend.log(_('comet failure') + ' [e=' + e.message + ']');
            }
        }
        cometRequest.delay(100);
    }
    ;

    cometRequest.delay(100);
};
