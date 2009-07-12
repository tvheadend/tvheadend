
/**
 * Comet interfaces
 */
Ext.extend(Comet = function() {
    this.addEvents({
	accessUpdate: true,
	dvbAdapter: true,
	dvbMux: true,
	dvbStore: true,
	dvbSatConf: true,
	logmessage: true,
	channeltags: true,
	autorec: true,
	dvrdb: true,
	channels: true,
    })
}, Ext.util.Observable);

tvheadend.comet = new Comet();


tvheadend.cometPoller = function() {

    var failures = 0;
    var boxid = null;

    var cometRequest = new Ext.util.DelayedTask(function() {

	Ext.Ajax.request({
	    url: 'comet',
	    params : boxid ? { boxid: boxid } : null,
	    success: function(result, request) { 
		parse_comet_response(result.responseText);

		if(failures > 2) {
		    tvheadend.log('Reconnected to Tvheadend',
				  'font-weight: bold; color: #080');
		}
		failures = 0;
	    },
	    failure: function(result, request) { 
		cometRequest.delay(failures > 10 ? 5000 : failures * 500);
		if(failures > 2) {
		    tvheadend.log('There seems to be a problem with the ' + 
				  'live update feed from Tvheadend. ' +
				  'Trying to reconnect...',
				  'font-weight: bold; color: #f00');
		}
		failures++;
	    }
	});
    });

    function parse_comet_response(responsetxt) {
	response = Ext.util.JSON.decode(responsetxt);
	boxid = response.boxid
	for(x = 0; x < response.messages.length; x++) {
            m = response.messages[x];
	    tvheadend.comet.fireEvent(m.notificationClass, m);
	}
	cometRequest.delay(100);
    };

    cometRequest.delay(100);
}


