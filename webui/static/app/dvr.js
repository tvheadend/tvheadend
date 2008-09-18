tvheadend.dvrStore = new Ext.data.JsonStore({
    root: 'entries',
    totalProperty: 'totalCount',
    fields: [
	{name: 'id'},
	{name: 'channel'},
	{name: 'title'},
	{name: 'description'},
	{name: 'chicon'},
        {name: 'start', type: 'date', dateFormat: 'U' /* unix time */},
        {name: 'end', type: 'date', dateFormat: 'U' /* unix time */},
	{name: 'status'},
	{name: 'schedstate'},
	{name: 'creator'},
        {name: 'duration'},
    ],
    url: 'dvrlist',
    autoLoad: true,
    id: 'id',
    remoteSort: true,
});

/**
 *
 */


tvheadend.dvrDetails = function(entry) {

    var content = '';
    var but;

    if(entry.chicon != null && entry.chicon.length > 0)
	content += '<img class="x-epg-chicon" src="' + entry.chicon + '">';

    content += '<div class="x-epg-title">' + entry.title + '</div>';
    content += '<div class="x-epg-desc">' + entry.description + '</div>';
    content += '<hr>'
    content += '<div class="x-epg-meta">Status: ' + entry.status + '</div>';


    var win = new Ext.Window({
	title: entry.title,
	bodyStyle: 'margin: 5px',
        layout: 'fit',
        width: 400,
        height: 300,
	constrainHeader: true,
	buttonAlign: 'center',
	html: content,
    });

    switch(entry.schedstate) {
    case 'sched':
	win.addButton({
	    handler: cancelEvent,
	    text: "Remove from schedule"
	});
	break;

    case 'rec':
	win.addButton({
	    handler: cancelEvent, 
	    text: "Abort recording"
	});
	break;
    }



    win.show();


    function cancelEvent() {
	Ext.Ajax.request({
	    url: '/dvr',
	    params: {entryId: entry.id, op: 'cancelEntry'},

	    success:function(response, options) {
		win.close();
	    },

	    failure:function(response, options) {
		Ext.MessageBox.alert('DVR', response.statusText);
	    }
	});
    }

}

/**
 *
 */
tvheadend.dvr = function() {

    function renderDate(value){
	var dt = new Date(value);
	return dt.format('l H:i');
    } 

   function renderDuration(value){
       value = value / 60; /* Nevermind the seconds */
       
       if(value >= 60) {
	   var min = value % 60;
	   var hours = parseInt(value / 60);

	   if(min == 0) {
	       return hours + ' hrs';
	   }
	   return hours + ' hrs, ' + min + ' min';
       } else {
	   return value + ' min';
       }
    } 

    var dvrCm = new Ext.grid.ColumnModel([
	{
	    width: 250,
	    id:'title',
	    header: "Title",
	    dataIndex: 'title',
	},{
	    width: 100,
	    id:'start',
	    header: "Start",
	    dataIndex: 'start',
	    renderer: renderDate,
	},{
	    width: 100,
	    hidden:true,
	    id:'end',
	    header: "End",
	    dataIndex: 'end',
	    renderer: renderDate,
	},{
	    width: 100,
	    id:'duration',
	    header: "Duration",
	    dataIndex: 'duration',
	    renderer: renderDuration
	},{
	    width: 250,
	    id:'channel',
	    header: "Channel",
	    dataIndex: 'channel',
	},{
	    width: 200,
	    id:'creator',
	    header: "Created by",
	    hidden:true,
	    dataIndex: 'creator',
	},{
	    width: 200,
	    id:'status',
	    header: "Status",
	    dataIndex: 'status',
	}
    ]);


    var panel = new Ext.grid.GridPanel({
        loadMask: true,
	stripeRows: true,
	disableSelection: true,
	title: 'Digital Video Recorder',
	store: tvheadend.dvrStore,
	cm: dvrCm,
        viewConfig: {forceFit:true},
	
        bbar: new Ext.PagingToolbar({
            store: tvheadend.dvrStore,
            pageSize: 20,
            displayInfo: true,
            displayMsg: 'Programs {0} - {1} of {2}',
            emptyMsg: "No programs to display"
        })

    });

    
    panel.on('rowclick', rowclicked);
    function rowclicked(grid, index) {
	new tvheadend.dvrDetails(grid.getStore().getAt(index).data);
    }


    return panel;
}

