tvheadend.tvhlog = function(panel, index) {

    function onchange(form, field, nval, oval) {
       var f = form.getForm();
       var enable_syslog = f.findField('enable_syslog');
       var debug_syslog = f.findField('syslog');
       if (debug_syslog.cbEl)
         debug_syslog.setDisabled(!enable_syslog.getValue() || enable_syslog.disabled);
       var trace = f.findField('trace');
       var tracesubs = f.findField('tracesubs');
       if (tracesubs.cbEl)
         tracesubs.setDisabled(!trace.getValue() || trace.disabled);
    }

    //START SUBSYSTEM SELECTION WINDOW CODE

    //Define some variables local to this module.
    let debugSubsystemsSaved = "";
    let traceSubsystemsSaved = "";

    let debugBoxUI = {};
    let traceBoxUI = {};

    let subsystemStore = {};

    //Create a button to show in the TVH config/debug toolbar.
    //When pressed, this will display the subsystem selection window.
    let subsystemsButton = {
        name: 'subsys',
        builder: function() {
            return new Ext.Toolbar.Button({
                tooltip: _('Select trace/debug options from a list of subsystems.'),
                iconCls: 'list',
                text: _('Select Subsystems'),
                disabled: false
            });
        },
        callback: function(conf, e, store, select) {

            //Get a list of the subsystems currently entered
            //Do it this way instead of looking at the current status from
            //the API because the user may be in the middle of a manual change
            //and it would be polite to preserve those manual changes.
            let debugBox = document.getElementsByName("debugsubs");
            let traceBox = document.getElementsByName("tracesubs");

            //These are the existing text UI text boxes.
            debugBoxUI = debugBox[0];
            traceBoxUI = traceBox[0];

            //Save the initial UI text box values for a potential
            //reset at a later time.
            debugSubsystemsSaved = debugBoxUI.value;
            traceSubsystemsSaved = traceBoxUI.value;

            //Break down the existing text box values into arrays.
            let debugArray = debugBoxUI.value.split(",");
            let traceArray = traceBoxUI.value.split(",");

            //Grab the list of subsystems from the server.
            //Note that this is an asynchronous call and the
            //rest of the module needs to be contained within
            //the 'success' method in order to work.
            let subsystemList = Ext.Ajax.request({
                url: 'api/tvhlog/subsystem/grid',
                success: function(d) {

                    //Parse the results of the call and extract the 'entries'.
                    let subsystemResponse = JSON.parse(d.responseText);
                    subsystemList = subsystemResponse.entries;

                    //Add 'all' to the beginning of the list.
                    //Subsystem 'all' is not a valid subsystem per-se,
                    //however, TVH uses it internally to enable all subsystems.
                    let tempSub = {
                        id: -1,
                        subsystem: 'all',
                        description: _('All subsystems'),
                        debug: false,
                        trace: false
                    };
                    subsystemList.unshift(tempSub);

                    //Load the starting conditions from the text boxes
                    //because the user may have already made some manual changes.
                    let i = 0;

                    while (i < subsystemList.length) {
                        subsystemList[i].debug = debugArray.includes(subsystemList[i].subsystem);
                        subsystemList[i].trace = traceArray.includes(subsystemList[i].subsystem);
                        i++;
                    }

                    //Create the reset buttons for the bottom of the window.
                    let buttons = [];
                    buttons.push(new Ext.Toolbar.Button({
                        handler: function() {
                            windowButtons(10)
                        },
                        iconCls: 'delete',
                        text: _('Clear Debug'),
                        tooltip: _('Clear all debug subsystems.'),
                    }));
                    buttons.push(new Ext.Toolbar.Button({
                        handler: function() {
                            windowButtons(11)
                        },
                        iconCls: 'delete',
                        text: _('Clear Trace'),
                        tooltip: _('Clear all trace subsystems.'),
                    }));
                    buttons.push(new Ext.Toolbar.Button({
                        handler: function() {
                            windowButtons(0)
                        },
                        iconCls: 'undo',
                        text: _('Restore Debug'),
                        tooltip: _('Restore the debug subsystems to their initial state.'),
                    }));
                    buttons.push(new Ext.Toolbar.Button({
                        handler: function() {
                            windowButtons(1)
                        },
                        iconCls: 'undo',
                        text: _('Restore Trace'),
                        tooltip: _('Restore the trace subsystems to their initial state.'),
                    }));

                    //This array will contain all of the elements to be
                    //rendered onto the subsystem selection window.
                    let windowItems = [];

                    //Create the data store that feeds the grid.
                    //It takes the JSON response from TVH as its source.
                    subsystemStore = new Ext.data.JsonStore({
                        root: 'entries',
                        data: subsystemResponse,
                        autoLoad: true,
                        id: 'id',
                        fields: ['debug', 'trace', 'subsystem', 'description'],
                        remoteSort: false,
                    });

                    //Take a boolean field and display checkboxes instead
                    //using existing ExtJS styles to match the TVH look-and-feel.
                    function subsystemBooleanRenderer(val) {
                        if (val) {
                            return '<div class="x-grid3-check-col-on">&nbsp;</div>';
                        } else {
                            return '<div class="x-grid3-check-col">&nbsp;</div>';
                        }
                    }

                    //Define the column mode to be used.  Note the custom
                    //rendering for the boolean fields.
                    let subsystemColumnModel = new Ext.grid.ColumnModel({
                        columns: [{
                                header: _("Debug"),
                                width: 50,
                                id: 'debug',
                                renderer: subsystemBooleanRenderer
                            },
                            {
                                header: _("Trace"),
                                width: 50,
                                id: 'trace',
                                renderer: subsystemBooleanRenderer
                            },
                            {
                                header: _("Subsystem"),
                                width: 80,
                                id: 'subsystem'
                            },
                            {
                                header: _("Description"),
                                width: 280,
                                id: 'description'
                            }
                        ],
                        defaults: {
                            sortable: false,
                            menuDisabled: true,
                            width: 100
                        }
                    });


                    //Read through all of the subsystems and build a UI string.
                    function getSubsystemString(checkList) {
                        let debugList = [];
                        let traceList = [];

                        i = 0;
                        while (i < checkList.length) {

                            if (checkList[i].debug) {
                                debugList.push(checkList[i].subsystem);
                            }

                            if (checkList[i].trace) {
                                traceList.push(checkList[i].subsystem);
                            }

                            i++;
                        }

                        return {
                            debug: debugList.toString(),
                            trace: traceList.toString()
                        };

                    } //END getSubsystemString


                    //Take the supplied strings and update the UI text boxes.
                    function updateSubsystemUI(stringValues) {
                        debugBoxUI.value = stringValues.debug;
                        traceBoxUI.value = stringValues.trace;
                    }

                    //Taking the row and column coordinates of the grid cell,
                    //update the boolean values for each debug/trace and then
                    //update the UI.
                    function updateSubsystemActions(row, column) {

                        let myRecord;

                        //Toggle the debug subsystem
                        if (column == 0) {
                            subsystemList[row].debug = !subsystemList[row].debug;
                            myRecord = subsystemStore.getAt(row);
                            myRecord.set('debug', subsystemList[row].debug);
                        }

                        //Toggle the trace subsystem
                        if (column == 1) {
                            subsystemList[row].trace = !subsystemList[row].trace;
                            myRecord = subsystemStore.getAt(row);
                            myRecord.set('trace', subsystemList[row].trace);
                        }

                        //Toggle both subsystems
                        if (column == 2) {

                            //If either is off, turn them both on
                            if (!subsystemList[row].debug || !subsystemList[row].trace) {
                                subsystemList[row].debug = true;
                                subsystemList[row].trace = true;
                            } else {
                                subsystemList[row].debug = false;
                                subsystemList[row].trace = false;
                            }

                            myRecord = subsystemStore.getAt(row);
                            myRecord.set('debug', subsystemList[row].debug);
                            myRecord.set('trace', subsystemList[row].trace);
                        }

                        //Update the existing UI text fields with the new values.
                        updateSubsystemUI(getSubsystemString(subsystemList));
                    } //END updateSubsystemActions

                    //Reset the subsystem selection to its original value.
                    function resetSubsystems(propertyName, textBoxUI, newValues) {

                        let newArray = newValues.split(",");

                        //Reset the text box to the previous values
                        textBoxUI.value = newValues;

                        //Loop through all of the subsystems and update their state
                        //in the array and the store based on the saved state.
                        let i = 0;
                        while (i < subsystemList.length) {

                            subsystemList[i][propertyName] = newArray.includes(subsystemList[i].subsystem);
                            let myRecord = subsystemStore.getAt(i);
                            myRecord.set(propertyName, subsystemList[i][propertyName]);
                            i++;
                        } //END while loop
                    } //END reset subsystems


                    //Handle the buttons at the bottom of the window.
                    function windowButtons(button) {

                        if (button == 0) {
                            resetSubsystems("debug", debugBoxUI, debugSubsystemsSaved);
                        }

                        if (button == 1) {
                            resetSubsystems("trace", traceBoxUI, traceSubsystemsSaved);
                        }

                        if (button == 10) {
                            resetSubsystems("debug", debugBoxUI, "");
                        }

                        if (button == 11) {
                            resetSubsystems("trace", traceBoxUI, "");
                        }


                    } //End window button handlers.


                    //Create the grid to be shown on the subsystem selection window.
                    let grid = new Ext.grid.GridPanel({
                        flex: 1,
                        autoWidth: true,
                        autoScroll: true,
                        stripeRows: true,
                        store: subsystemStore,
                        cm: subsystemColumnModel,
                        border: false,
                        viewConfig: {
                            markDirty: false //Hide the red tag that highlights changed records.
                        },
                        listeners: {
                            cellclick: function(gridView, htmlElement, columnIndex, dataRecord) {
                                updateSubsystemActions(htmlElement, columnIndex);
                            }
                        } //END listeners
                    });

                    //Add the grid to the list of items to be rendered on the window.
                    windowItems.push(grid);

                    //Create a window to display the list of subsystems.
                    let windowHeight = Ext.getBody().getViewSize().height - 150;
                    let subsysWin = new Ext.Window({
                        title: _('Select Subsystems'),
                        iconCls: 'debug',
                        layout: 'fit',
                        width: 500,
                        height: windowHeight,
                        constrainHeader: true,
                        buttons: buttons,
                        buttonAlign: 'center',
                        autoScroll: true,
                        items: windowItems
                    });

                    //Display the subsystem selection window.
                    subsysWin.show();

                }, //END AJAX OK
                failure: function(response, options) {
                    Ext.MessageBox.alert(_('Listing Subsystems'), response.statusText);
                    return "";
                }
            }); //END Ajax request for subsystems list.
        } //END button callback.
    }; //END button object.

    //END SUBSYSTEM SELECTION WINDOW CODE

    //This is the standard debug config screen.
    tvheadend.idnode_simple(panel, {
        url: 'api/tvhlog/config',
        title: _('Configuration'),
        iconCls: 'debug',
        tbar: [subsystemsButton],   //Add the button to list the subsystems
        tabIndex: index,
        comet: 'tvhlog_conf',
        labelWidth: 180,
        width: 530,
        onchange: onchange,
        saveText: _("Apply configuration (run-time only)"),
        saveTooltip: _('Apply any changes made below to the run-time configuration.') + '<br/>' +
                     _('They will be lost when the application next restarts.')
    });

};

tvheadend.memoryinfo = function(panel, index)
{
    tvheadend.idnode_grid(panel, {
        url: 'api/memoryinfo',
        titleS: _('Memory Information Entry'),
        titleP: _('Memory Information Entries'),
        iconCls: 'exclamation',
        tabIndex: index,
        uilevel: 'expert',
        readonly: true
    });
};
