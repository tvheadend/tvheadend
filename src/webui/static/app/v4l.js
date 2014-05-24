/**
 * V4L adapter details
 */
tvheadend.v4l_adapter_general = function(adapterData) {

    adapterId = adapterData.identifier;

    /* Conf panel */

    var confreader = new Ext.data.JsonReader({
        root: 'v4ladapters'
    }, ['name', 'logging']);

    function saveConfForm() {
        confform.getForm().submit({
            url: 'v4l/adapter/' + adapterId,
            params: {
                'op': 'save'
            },
            waitMsg: 'Saving Data...'
        });
    }

    var items = [{
            fieldLabel: 'Adapter name',
            name: 'name',
            width: 250
        }, new Ext.form.Checkbox({
            fieldLabel: 'Detailed logging',
            name: 'logging'
        })];

    var confform = new Ext.FormPanel({
        title: 'Adapter configuration',
        columnWidth: .40,
        frame: true,
        border: true,
        disabled: true,
        style: 'margin:10px',
        bodyStyle: 'padding:5px',
        labelAlign: 'right',
        labelWidth: 110,
        waitMsgTarget: true,
        reader: confreader,
        defaultType: 'textfield',
        items: items,
        buttons: [{
                text: 'Save',
                handler: saveConfForm
            }]
    });

    confform.getForm().load({
        url: 'v4l/adapter/' + adapterId,
        params: {
            'op': 'load'
        },
        success: function(form, action) {
            confform.enable();
        }
    });

    /**
     * Information / capabilities panel 
     */

    var infoTemplate = new Ext.XTemplate(
            '<h2 style="font-size: 150%">Hardware</h2>'
            + '<h3>Device path:</h3>{path}' + '<h3>Device name:</h3>{devicename}'
            + '<h2 style="font-size: 150%">Status</h2>'
            + '<h3>Currently tuned to:</h3>{currentMux}&nbsp');

    var infoPanel = new Ext.Panel({
        title: 'Information and capabilities',
        columnWidth: .35,
        frame: true,
        border: true,
        style: 'margin:10px',
        bodyStyle: 'padding:5px',
        html: infoTemplate.applyTemplate(adapterData)
    });

    /**
     * Main adapter panel
     */
    var panel = new Ext.Panel({
        title: 'General',
        layout: 'column',
        items: [confform, infoPanel]
    });

    /**
     * Subscribe and react on updates for this adapter
     */
    tvheadend.tvAdapterStore.on('update', function(s, r, o) {
        if (r.data.identifier !== adapterId)
            return;
        infoTemplate.overwrite(infoPanel.body, r.data);
    });

    return panel;
};

/**
 * V4L service grid
 */
tvheadend.v4l_services = function(adapterId) {

    var fm = Ext.form;

    var enabledColumn = new Ext.grid.CheckColumn({
        header: "Enabled",
        dataIndex: 'enabled',
        width: 45
    });

    var cm = new Ext.grid.ColumnModel({
        defaultSortable: true,
        columns: [
            enabledColumn, {
                header: "Channel name",
                dataIndex: 'channelname',
                width: 150,
                renderer: function(value, metadata, record, row, col, store) {
                    return value ? value : '<span class="tvh-grid-unset">Unmapped</span>';
                },
                editor: new fm.ComboBox({
                    store: tvheadend.channels,
                    allowBlank: true,
                    typeAhead: true,
                    minChars: 2,
                    lazyRender: true,
                    triggerAction: 'all',
                    mode: 'local',
                    displayField: 'name'
                })
            }, {
                header: "Frequency",
                dataIndex: 'frequency',
                width: 60,
                editor: new fm.NumberField({
                    minValue: 10000,
                    maxValue: 1000000000
                })
            }]});

    var rec = Ext.data.Record.create(['id', 'enabled', 'channelname',
        'frequency']);

    var store = new Ext.data.JsonStore({
        root: 'entries',
        fields: rec,
        url: "v4l/services/" + adapterId,
        autoLoad: true,
        id: 'id',
        baseParams: {
            op: "get"
        },
        listeners: {
            'update': function(s, r, o) {
                d = s.getModifiedRecords().length === 0;
                saveBtn.setDisabled(d);
                rejectBtn.setDisabled(d);
            }
        }
    });

    function addRecord() {
        Ext.Ajax.request({
            url: "v4l/services/" + adapterId,
            params: {
                op: "create"
            },
            failure: function(response, options) {
                Ext.MessageBox.alert('Server Error',
                        'Unable to generate new record');
            },
            success: function(response, options) {
                var responseData = Ext.util.JSON.decode(response.responseText);
                var p = new rec(responseData, responseData.id);
                grid.stopEditing();
                store.insert(0, p);
                grid.startEditing(0, 0);
            }
        });
    };

    function delSelected() {
        var selectedKeys = grid.selModel.selections.keys;
        if (selectedKeys.length > 0) {
            Ext.MessageBox.confirm('Message',
                    'Do you really want to delete selection?', deleteRecord);
        }
        else {
            Ext.MessageBox.alert('Message',
                    'Please select at least one item to delete');
        }
    };

    function deleteRecord(btn) {
        if (btn === 'yes') {
            var selectedKeys = grid.selModel.selections.keys;

            Ext.Ajax.request({
                url: "v4l/services/" + adapterId,
                params: {
                    op: "delete",
                    entries: Ext.encode(selectedKeys)
                },
                failure: function(response, options) {
                    Ext.MessageBox.alert('Server Error', 'Unable to delete');
                },
                success: function(response, options) {
                    store.reload();
                }
            });
        }
    }

    function saveChanges() {
        var mr = store.getModifiedRecords();
        var out = new Array();
        for (var x = 0; x < mr.length; x++) {
            v = mr[x].getChanges();
            out[x] = v;
            out[x].id = mr[x].id;
        }

        Ext.Ajax.request({
            url: "v4l/services/" + adapterId,
            params: {
                op: "update",
                entries: Ext.encode(out)
            },
            success: function(response, options) {
                store.commitChanges();
            },
            failure: function(response, options) {
                Ext.MessageBox.alert('Message', response.statusText);
            }
        });
    }

    var delButton = new Ext.Toolbar.Button({
        tooltip: 'Delete one or more selected rows',
        iconCls: 'remove',
        text: 'Delete selected services',
        handler: delSelected,
        disabled: true
    });

    var saveBtn = new Ext.Toolbar.Button({
        tooltip: 'Save any changes made (Changed cells have red borders).',
        iconCls: 'save',
        text: "Save changes",
        handler: saveChanges,
        disabled: true
    });

    var rejectBtn = new Ext.Toolbar.Button({
        tooltip: 'Revert any changes made (Changed cells have red borders).',
        iconCls: 'undo',
        text: "Revert changes",
        handler: function() {
            store.rejectChanges();
        },
        disabled: true
    });

    var selModel = new Ext.grid.RowSelectionModel({
        singleSelect: false
    });

    var grid = new Ext.grid.EditorGridPanel({
        stripeRows: true,
        title: 'Services',
        plugins: [enabledColumn],
        store: store,
        clicksToEdit: 2,
        cm: cm,
        viewConfig: {
            forceFit: true
        },
        selModel: selModel,
        tbar: [
            {
                tooltip: 'Create a new entry on the server. '
                        + 'The new entry is initially disabled so it must be enabled '
                        + 'before it start taking effect.',
                iconCls: 'add',
                text: 'Add service',
                handler: addRecord
            }, '-', delButton, '-', saveBtn, rejectBtn]
    });

    store.on('update', function(s, r, o) {
        d = s.getModifiedRecords().length === 0;
        saveBtn.setDisabled(d);
        rejectBtn.setDisabled(d);
    });

    selModel.on('selectionchange', function(self) {
        delButton.setDisabled(self.getCount() === 0);
    });

    return grid;
};

/**
 *
 */
tvheadend.v4l_adapter = function(data) {
    var panel = new Ext.TabPanel({
        border: false,
        activeTab: 0,
        autoScroll: true,
        items: [new tvheadend.v4l_adapter_general(data),
            new tvheadend.v4l_services(data.identifier)]
    });
    return panel;
};
