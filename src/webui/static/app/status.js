/**
 *
 */
tvheadend.status_subs = function() {

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
			name : 'in'
		}, {
			name : 'out'
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
			r.data.in       = m.in;
			r.data.out      = m.out;

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
		id : 'in',
		header : "Input (kb/s)",
		dataIndex : 'in',
		renderer: renderBw
	}, {
		width : 50,
		id : 'out',
		header : "Output (kb/s)",
		dataIndex : 'out',
		renderer: renderBw
	} ]);

	var subs = new Ext.grid.GridPanel({
                border: false,
		loadMask : true,
		stripeRows : true,
		disableSelection : true,
		title : 'Active subscriptions',
		iconCls : 'eye',
		store : tvheadend.subsStore,
		cm : subsCm,
                flex: 1,
		viewConfig : {
			forceFit : true
		}
	});
        return subs;
}


/**
 * Streams
 */
tvheadend.status_streams = function() {

	var stream_store = new Ext.data.JsonStore({
		root : 'entries',
		totalProperty : 'totalCount',
		fields : [ {
			name : 'uuid'
		}, {
			name : 'input'
		}, {
			name : 'username'
		}, {
			name : 'stream'
		}, {
			name : 'subs'
		}, {
			name : 'weight'
		}, {
			name : 'signal'
		}, {
			name : 'ber'
		}, {
			name : 'unc'
		}, {
			name : 'snr'
		}, {
			name : 'bps'
		},
		],
		url : 'api/input/status',
		autoLoad : true,
		id : 'uuid'
	});

  tvheadend.comet.on('input_status', function(m){
    if (m.reload != null) stream_store.reload();
    if (m.update != null) {
      var r = stream_store.getById(m.uuid);
      if (r) {
        r.data.subs    = m.subs;
        r.data.weight  = m.weight;
        r.data.signal  = m.signal;
        r.data.ber     = m.ber;
        r.data.unc     = m.unc;
        r.data.snr     = m.snr;
        r.data.bps     = m.bps;
			  stream_store.afterEdit(r);
			  stream_store.fireEvent('updated', stream_store, r, Ext.data.Record.COMMIT);
      } else {
        stream_store.reload();
      } 
    }
  });

	var signal = new Ext.ux.grid.ProgressColumn({
		header : "Signal Strength",
		dataIndex : 'signal',
		width : 85,
		textPst : '%',
		colored : true
	});

	function renderBw(value) {
		return parseInt(value / 1024);
	}

	var cm = new Ext.grid.ColumnModel([{
		width : 100,
		header : "Input",
		dataIndex : 'input'
        },{
		width : 100,
		header : "Stream",
		dataIndex : 'stream'
        },{
		width : 50,
		header : "Subs #",
		dataIndex : 'subs'
        },{
		width : 50,
		header : "Weight",
		dataIndex : 'weight'
        },{
		width : 100,
		header : "Bandwidth (kb/s)",
		dataIndex : 'bps',
		renderer: renderBw
        },{
		width : 50,
		header : "Bit error rate",
		dataIndex : 'ber'
        },{
		width : 50,
		header : "Uncorrected bit error rate",
		dataIndex : 'unc'
        },{
		width : 50,
		header : "SNR",
		dataIndex : 'snr',
                renderer: function(value) {
                        if(value > 0) {
                                return value.toFixed(1) + " dB";
                        } else {
                                return '<span class="tvh-grid-unset">Unknown</span>';
                        }
                }
        }, signal]);

	var panel = new Ext.grid.GridPanel({
                border: false,
		loadMask : true,
		stripeRows : true,
		disableSelection : true,
		title : 'Stream',
		iconCls : 'hardware',
		store : stream_store,
		cm : cm,
                flex: 1,
		viewConfig : {
			forceFit : true
		}
	});
        return panel;
}

tvheadend.status = function() {

        var panel = new Ext.Panel({
                border: false,
		layout : 'vbox',
		title : 'Status',
		iconCls : 'eye',
		items : [ new tvheadend.status_subs, new tvheadend.status_streams ]
        });

	return panel;
}

