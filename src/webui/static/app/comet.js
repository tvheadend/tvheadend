
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
    
    function parse_comet_response(responsetxt) {
	response = Ext.util.JSON.decode(responsetxt);
	for(x = 0; x < response.messages.length; x++) {
            m = response.messages[x];
	    tvheadend.comet.fireEvent(m.notificationClass, m);
	}

	Ext.Ajax.request({
	    url: '/comet',
	    params : { boxid: response.boxid },
	    success: function(result, request) { 
		parse_comet_response(result.responseText);
	    },
	    failure: function(result, request) { 
		tvheadend.log('Connection to server lost' + 
			      ', please reload user interface',
			      'font-weight: bold; color: #f00');
	    }
	});
	
    };

    Ext.Ajax.request({
	url: '/comet',
	success: function(result, request) { 
	    parse_comet_response(result.responseText);
	}
    });
}


