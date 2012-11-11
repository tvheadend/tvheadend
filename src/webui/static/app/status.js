/**
 *
 */
tvheadend.status = function() {

	tvheadend.subsStore = new Ext.data.JsonStore({
		root : 'entries',
		totalProperty : 'totalCount',
		fields : [ {
			name : 'id'
		}, {
			name : 'hostname'
		}, {
			name : 'username'
		}, {
			name : 'title'
		}, {
			name : 'channel'
		}, {
			name : 'service'
		}, {
			name : 'state'
		}, {
			name : 'errors'
		}, {
			name : 'bw'
		}, {
			name : 'start',
			type : 'date',
			dateFormat : 'U' /* unix time */
		} ],
		url : 'subscriptions',
		autoLoad : true,
		id : 'id'
	});



	tvheadend.comet.on('subscriptions', function(m) {

		if (m.reload != null) tvheadend.subsStore.reload();

		if (m.updateEntry != null) {
			r = tvheadend.subsStore.getById(m.id)
			if (typeof r === 'undefined') {
				tvheadend.subsStore.reload();
				return;
			}

			r.data.channel  = m.channel;
			r.data.service  = m.service;
			r.data.state    = m.state;
			r.data.errors   = m.errors;
			r.data.bw       = m.bw

			tvheadend.subsStore.afterEdit(r);
			tvheadend.subsStore.fireEvent('updated', tvheadend.subsStore, r,
				Ext.data.Record.COMMIT);
		}
	});

	function renderDate(value) {
		var dt = new Date(value);
		return dt.format('D j M H:i');
	}

	function renderBw(value) {
		return parseInt(value / 125);
	}

	var subsCm = new Ext.grid.ColumnModel([{
		width : 50,
		id : 'hostname',
		header : "Hostname",
		dataIndex : 'hostname'
	}, {
		width : 50,
		id : 'username',
		header : "Username",
		dataIndex : 'username'
	}, {
		width : 80,
		id : 'title',
		header : "Title",
		dataIndex : 'title'
	}, {
		width : 50,
		id : 'channel',
		header : "Channel",
		dataIndex : 'channel'
	}, {
		width : 200,
		id : 'service',
		header : "Service",
		dataIndex : 'service',
	}, {
		width : 50,
		id : 'start',
		header : "Start",
		dataIndex : 'start',
		renderer : renderDate
	}, {
		width : 50,
		id : 'state',
		header : "State",
		dataIndex : 'state'
	}, {
		width : 50,
		id : 'errors',
		header : "Errors",
		dataIndex : 'errors'
	}, {
		width : 50,
		id : 'bw',
		header : "Bandwidth (kb/s)",
		dataIndex : 'bw',
		renderer: renderBw
	} ]);

	var panel = new Ext.grid.GridPanel({
		loadMask : true,
		stripeRows : true,
		disableSelection : true,
		title : 'Active subscriptions',
		iconCls : 'eye',
		store : tvheadend.subsStore,
		cm : subsCm,
		viewConfig : {
			forceFit : true
		}
	});
	return panel;
}

