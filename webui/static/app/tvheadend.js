
/**
 * Displays a help popup window
 */
tvheadend.help = function(title, pagename) {
    Ext.Ajax.request({
	url: '/docs/' + pagename,
	success: function(result, request) { 

	    var content = new Ext.Panel({
		autoScroll:true,
		border: false,
		layout:'fit', 
		html: result.responseText
	    });

	    var win = new Ext.Window({
		title: 'Help for ' + title,
		layout: 'fit',
		width: 900,
		height: 400,
		constrainHeader: true,
		items: [content]
	    });    
	    win.show();

	}});
}


/**
 * This function creates top level tabs based on access so users without 
 * access to subsystems won't see them.
 *
 * Obviosuly, access is verified in the server too.
 */
function accessUpdate(o) {


    if(o.dvr == true && tvheadend.dvrpanel == null) {
	tvheadend.dvrpanel = new tvheadend.dvr;
	tvheadend.rootTabPanel.add(tvheadend.dvrpanel);
    }

    if(o.admin == true && tvheadend.confpanel == null) {
	tvheadend.confpanel = new Ext.TabPanel({
	    activeTab:0, 
	    autoScroll:true, 
	    title: 'Configuration', 
	    items: [new tvheadend.chconf,
		    new tvheadend.xmltv,
		    new tvheadend.cteditor,
		    new tvheadend.dvrsettings,
		    new tvheadend.dvb,
		    new tvheadend.acleditor, 
		    new tvheadend.cwceditor]
	});
	tvheadend.rootTabPanel.add(tvheadend.confpanel);
    }

    tvheadend.aboutPanel = new Ext.Panel({
	border: false,
	layout:'fit', 
	title:'About',
	autoLoad: '/about.html'
    });

    tvheadend.rootTabPanel.add(tvheadend.aboutPanel);
    

    tvheadend.rootTabPanel.doLayout();
}

/**
 * Comet interfaces
 */
tvheadend.comet_poller = function() {
    
    function parse_comet_response(responsetxt) {
	
	var response = Ext.util.JSON.decode(responsetxt);
	
	for (var x = 0; x < response.messages.length; x++) {
	    var m = response.messages[x];
	    
	    switch(m.notificationClass) {

	    case 'accessUpdate':
		accessUpdate(m);
		break;

	    case 'channeltags':
		if(m.reload != null)
		    tvheadend.channelTags.reload();
		break;

	    case 'autorec':
		if(m.asyncreload != null)
		    tvheadend.autorecStore.reload();
		break;

	    case 'dvrdb':
		if(m.reload != null)
		    tvheadend.dvrStore.reload();
		break;

	    case 'channels':
		if(m.reload != null)
		    tvheadend.channels.reload();
		break;

	    case 'logmessage':
		
		var sl = Ext.get('systemlog');
		var e = Ext.DomHelper.append(sl,
		    '<div>' + m.logtxt + '</div>');
		e.scrollIntoView(sl);
		break;
		
	    case 'dvbadapter':
	    case 'dvbmux':
	    case 'dvbtransport':
		var n = tvheadend.dvbtree.getNodeById(m.id);
		if(n != null) {

		    if(m.reload != null && n.isLoaded()) {
			n.reload();
		    }
		    
		    if(m.name != null) {
			n.setText(m.name);
			n.attributes.name = m.name;
		    }
		    
		    if(m.quality != null) {
			n.getUI().setColText(3, m.quality);
			n.attributes.quality = m.quality;
		    }

		    if(m.status != null) {
			n.getUI().setColText(2, m.status);
			n.attributes.status = m.status;
		    }
		}
		break;
		
	    }
	}
	
	Ext.Ajax.request({
	    url: '/comet',
	    params : { boxid: response.boxid },
	    success: function(result, request) { 
		parse_comet_response(result.responseText);
	    }});
	
    };
    
    Ext.Ajax.request({
	url: '/comet',
	success: function(result, request) { 
	    parse_comet_response(result.responseText);
	}});
}


/**
 *
 */

// create application
tvheadend.app = function() {
 
    // public space
    return {
 
        // public methods
        init: function() {
	    

	    tvheadend.rootTabPanel = new Ext.TabPanel({
		region:'center',
		activeTab:0,
		items:[new tvheadend.epg]
	    });

	    var viewport = new Ext.Viewport({
		layout:'border',
		items:[
		    {
			region:'south',
			contentEl: 'systemlog',
			split:true,
			autoScroll:true,
			height: 150,
			minSize: 100,
			maxSize: 400,
			collapsible: true,
			title:'System log',
			margins:'0 0 0 0'
		    },tvheadend.rootTabPanel
		]
	    });


	    new tvheadend.comet_poller;
	    Ext.QuickTips.init();
	}
	
    };
}(); // end of app
 
