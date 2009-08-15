
/**
 * Displays a help popup window
 */
tvheadend.help = function(title, pagename) {
    Ext.Ajax.request({
	url: 'docs/' + pagename,
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
	    iconCls: 'wrench',
	    items: [new tvheadend.chconf,
		    new tvheadend.xmltv,
		    new tvheadend.cteditor,
		    new tvheadend.dvrsettings,
		    new tvheadend.dvb,
		    new tvheadend.iptv,
		    new tvheadend.acleditor, 
		    new tvheadend.cwceditor]
	});
	tvheadend.rootTabPanel.add(tvheadend.confpanel);
    }

    if(tvheadend.aboutPanel == null) {
	tvheadend.aboutPanel = new Ext.Panel({
	    border: false,
	    layout:'fit', 
	    title:'About',
	    iconCls:'info',
	    autoLoad: 'about.html'
	});
	tvheadend.rootTabPanel.add(tvheadend.aboutPanel);
    }

    tvheadend.rootTabPanel.doLayout();
}


/**
*
*/
tvheadend.log = function(msg, style) {
    s = style ? '<div style="' + style + '">' : '<div>'

    sl = Ext.get('systemlog');
    e = Ext.DomHelper.append(sl, s + '<pre>' + msg + '</pre></div>');
    e.scrollIntoView('systemlog');
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

	    tvheadend.comet.on('accessUpdate', accessUpdate);

	    tvheadend.comet.on('logmessage', function(m) {
		tvheadend.log(m.logtxt);
	    });

	    new tvheadend.cometPoller;

	    Ext.QuickTips.init();
	}
	
    };
}(); // end of app
 
