/*
 * Ext JS Library 2.1
 * Copyright(c) 2006-2008, Ext JS, LLC.
 * licensing@extjs.com
 * 
 * http://extjs.com/license
 */




/*
 * Ext JS Library 2.2
 * Copyright(c) 2006-2008, Ext JS, LLC.
 * licensing@extjs.com
 * 
 * http://extjs.com/license
 */

/*
 * Note that this control should still be treated as an example and that the API will most likely
 * change once it is ported into the Ext core as a standard form control.  This is still planned
 * for a future release, so this should not yet be treated as a final, stable API at this time.
 */

/** 
 * @class Ext.ux.MultiSelect
 * @extends Ext.form.Field
 * A control that allows selection and form submission of multiple list items. The MultiSelect control
 * depends on the Ext.ux.DDView class to provide drag/drop capability both within the list and also 
 * between multiple MultiSelect controls (see the Ext.ux.ItemSelector).
 * 
 *  @history
 *    2008-06-19 bpm Original code contributed by Toby Stuart
 *    2008-06-19 bpm Docs and demo code clean up
 * 
 * @constructor
 * Create a new MultiSelect
 * @param {Object} config Configuration options
 */
Ext.ux.Multiselect = Ext.extend(Ext.form.Field, {
    /**
     * @cfg {String} legend Wraps the object with a fieldset and specified legend.
     */
    /**
     * @cfg {Store} store The {@link Ext.data.Store} used by the underlying Ext.ux.DDView.
     */
    /**
     * @cfg {Ext.ux.DDView} view The Ext.ux.DDView used to render the multiselect list.
     */
    /**
     * @cfg {String/Array} dragGroup The ddgroup name(s) for the DDView's DragZone (defaults to undefined). 
     */
    /**
     * @cfg {String/Array} dropGroup The ddgroup name(s) for the DDView's DropZone (defaults to undefined). 
     */
    /**
     * @cfg {Object/Array} tbar The top toolbar of the control. This can be a {@link Ext.Toolbar} object, a 
     * toolbar config, or an array of buttons/button configs to be added to the toolbar.
     */
    /**
     * @cfg {String} fieldName The name of the field to sort by when sorting is enabled.
     */
    /**
     * @cfg {String} appendOnly True if the list should only allow append drops when drag/drop is enabled 
     * (use for lists which are sorted, defaults to false).
     */
    appendOnly: false,
    /**
     * @cfg {Array} dataFields Inline data definition when not using a pre-initialised store. Known to cause problems 
     * in some browswers for very long lists. Use store for large datasets.
     */
    dataFields: [],
    /**
     * @cfg {Array} data Inline data when not using a pre-initialised store. Known to cause problems in some 
     * browswers for very long lists. Use store for large datasets.
     */
    data: [],
    /**
     * @cfg {Number} width Width in pixels of the control (defaults to 100).
     */
    width: 100,
    /**
     * @cfg {Number} height Height in pixels of the control (defaults to 100).
     */
    height: 100,
    /**
     * @cfg {String/Number} displayField Name/Index of the desired display field in the dataset (defaults to 0).
     */
    displayField: 0,
    /**
     * @cfg {String/Number} valueField Name/Index of the desired value field in the dataset (defaults to 1).
     */
    valueField: 1,
    /**
     * @cfg {Boolean} allowBlank True to require at least one item in the list to be selected, false to allow no 
     * selection (defaults to true).
     */
    allowBlank: true,
    /**
     * @cfg {Number} minLength Minimum number of selections allowed (defaults to 0).
     */
    minLength: 0,
    /**
     * @cfg {Number} maxLength Maximum number of selections allowed (defaults to Number.MAX_VALUE). 
     */
    maxLength: Number.MAX_VALUE,
    /**
     * @cfg {String} blankText Default text displayed when the control contains no items (defaults to the same value as
     * {@link Ext.form.TextField#blankText}.
     */
    blankText: Ext.form.TextField.prototype.blankText,
    /**
     * @cfg {String} minLengthText Validation message displayed when {@link #minLength} is not met (defaults to 'Minimum {0} 
     * item(s) required').  The {0} token will be replaced by the value of {@link #minLength}.
     */
    minLengthText: 'Minimum {0} item(s) required',
    /**
     * @cfg {String} maxLengthText Validation message displayed when {@link #maxLength} is not met (defaults to 'Maximum {0} 
     * item(s) allowed').  The {0} token will be replaced by the value of {@link #maxLength}.
     */
    maxLengthText: 'Maximum {0} item(s) allowed',
    /**
     * @cfg {String} delimiter The string used to delimit between items when set or returned as a string of values
     * (defaults to ',').
     */
    delimiter: ',',
    // DDView settings
    copy: false,
    allowDup: false,
    allowTrash: false,
    focusClass: undefined,
    sortDir: 'ASC',
    // private
    defaultAutoCreate: {tag: "div"},
    // private
    initComponent: function() {
        Ext.ux.Multiselect.superclass.initComponent.call(this);
        this.addEvents({
            'dblclick': true,
            'click': true,
            'change': true,
            'drop': true
        });
    },
    // private
    onRender: function(ct, position) {
        Ext.ux.Multiselect.superclass.onRender.call(this, ct, position);

        var cls = 'ux-mselect';
        var fs = new Ext.form.FieldSet({
            renderTo: this.el,
            title: this.legend,
            height: this.height,
            width: this.width,
            style: "padding:0;",
            tbar: this.tbar
        });
        //if(!this.legend)fs.el.down('.'+fs.headerCls).remove();
        fs.body.addClass(cls);

        var tpl = '<tpl for="."><div class="' + cls + '-item';
        if (Ext.isIE || Ext.isIE7) {
            tpl += '" unselectable=on';
        } else {
            tpl += ' x-unselectable"';
        }
        tpl += '>{' + this.displayField + '}</div></tpl>';

        if (!this.store) {
            this.store = new Ext.data.SimpleStore({
                fields: this.dataFields,
                data: this.data
            });
        }

        this.view = new Ext.ux.DDView({
            multiSelect: true,
            store: this.store,
            selectedClass: cls + "-selected",
            tpl: tpl,
            allowDup: this.allowDup,
            copy: this.copy,
            allowTrash: this.allowTrash,
            dragGroup: this.dragGroup,
            dropGroup: this.dropGroup,
            itemSelector: "." + cls + "-item",
            isFormField: false,
            applyTo: fs.body,
            appendOnly: this.appendOnly,
            sortField: this.sortField,
            sortDir: this.sortDir
        });

        fs.add(this.view);

        this.view.on('click', this.onViewClick, this);
        this.view.on('beforeClick', this.onViewBeforeClick, this);
        this.view.on('dblclick', this.onViewDblClick, this);
        this.view.on('drop', function(ddView, n, dd, e, data) {
            return this.fireEvent("drop", ddView, n, dd, e, data);
        }, this);

        this.hiddenName = this.name;
        var hiddenTag = {tag: "input", type: "hidden", value: "", name: this.name};
        if (this.isFormField) {
            this.hiddenField = this.el.createChild(hiddenTag);
        } else {
            this.hiddenField = Ext.get(document.body).createChild(hiddenTag);
        }
        fs.doLayout();
    },
    // private
    initValue: Ext.emptyFn,
    // private
    onViewClick: function(vw, index, node, e) {
        var arrayIndex = this.preClickSelections.indexOf(index);
        if (arrayIndex !== -1)
        {
            this.preClickSelections.splice(arrayIndex, 1);
            this.view.clearSelections(true);
            this.view.select(this.preClickSelections);
        }
        this.fireEvent('change', this, this.getValue(), this.hiddenField.dom.value);
        this.hiddenField.dom.value = this.getValue();
        this.fireEvent('click', this, e);
        this.validate();
    },
    // private
    onViewBeforeClick: function(vw, index, node, e) {
        this.preClickSelections = this.view.getSelectedIndexes();
        if (this.disabled) {
            return false;
        }
    },
    // private
    onViewDblClick: function(vw, index, node, e) {
        return this.fireEvent('dblclick', vw, index, node, e);   },
    /**
     * Returns an array of data values for the selected items in the list. The values will be separated
     * by {@link #delimiter}.
     * @return {Array} value An array of string data values
     */
    getValue: function(valueField) {
        var returnArray = [];
        var selectionsArray = this.view.getSelectedIndexes();
        if (selectionsArray.length === 0) {
            return '';
        }
        for (var i = 0; i < selectionsArray.length; i++) {
            returnArray.push(this.store.getAt(selectionsArray[i]).get(((valueField != null) ? valueField : this.valueField)));
        }
        return returnArray.join(this.delimiter);
    },
    /**
     * Sets a delimited string (using {@link #delimiter}) or array of data values into the list.
     * @param {String/Array} values The values to set
     */
    setValue: function(values) {
        var index;
        var selections = [];
        this.view.clearSelections();
        this.hiddenField.dom.value = '';

        if (!values || (values === '')) {
            return;
        }

        if (!(values instanceof Array)) {
            values = values.split(this.delimiter);
        }
        for (var i = 0; i < values.length; i++) {
            index = this.view.store.indexOf(this.view.store.query(this.valueField,
                    new RegExp('^' + values[i] + '$', "i")).itemAt(0));
            selections.push(index);
        }
        this.view.select(selections);
        this.hiddenField.dom.value = this.getValue();
        this.validate();
    },
    // inherit docs
    reset: function() {
        this.setValue('');
    },
    // inherit docs
    getRawValue: function(valueField) {
        var tmp = this.getValue(valueField);
        if (tmp.length) {
            tmp = tmp.split(this.delimiter);
        }
        else {
            tmp = [];
        }
        return tmp;
    },
    // inherit docs
    setRawValue: function(values) {
        setValue(values);
    },
    // inherit docs
    validateValue: function(value) {
        if (value.length < 1) { // if it has no value
            if (this.allowBlank) {
                this.clearInvalid();
                return true;
            } else {
                this.markInvalid(this.blankText);
                return false;
            }
        }
        if (value.length < this.minLength) {
            this.markInvalid(String.format(this.minLengthText, this.minLength));
            return false;
        }
        if (value.length > this.maxLength) {
            this.markInvalid(String.format(this.maxLengthText, this.maxLength));
            return false;
        }
        return true;
    }
});

Ext.reg("multiselect", Ext.ux.Multiselect);



/**
 * Ext.ux.grid.ProgressColumn - Ext.ux.grid.ProgressColumn is a grid plugin that
 * shows a progress bar for a number between 0 and 100 to indicate some sort of
 * progress. The plugin supports all the normal cell/column operations including
 * sorting, editing, dragging, and hiding. It also supports special progression
 * coloring or standard Ext.ProgressBar coloring for the bar.
 *
 * @author Benjamin Runnels <kraven@kraven.org>
 * @copyright (c) 2008, by Benjamin Runnels
 * @date 06 June 2008
 * @version 1.1
 *
 * @license Ext.ux.grid.ProgressColumn is licensed under the terms of the Open
 *          Source LGPL 3.0 license. Commercial use is permitted to the extent
 *          that the code/component(s) do NOT become part of another Open Source
 *          or Commercially licensed development library or toolkit without
 *          explicit permission.
 *
 * License details: http://www.gnu.org/licenses/lgpl.html
 */

/**
 * 22/07/2014: ceiling support backported from version 1.2, by Kai Sommerfeld
 */
Ext.namespace('Ext.ux.grid');

Ext.ux.grid.ProgressColumn = function(config) {
    Ext.apply(this, config);
    this.renderer = this.renderer.createDelegate(this);
    this.addEvents('action');
    Ext.ux.grid.ProgressColumn.superclass.constructor.call(this);
};

Ext.extend(Ext.ux.grid.ProgressColumn, Ext.util.Observable, {
    /**
     * @cfg {Integer} upper limit for full progress indicator (defaults to 100)
     */
    ceiling : 100,
    /**
     * @cfg {String} colored determines whether use special progression coloring
     *      or the standard Ext.ProgressBar coloring for the bar (defaults to
     *      false)
     */
    textPst: '%',
    /**
     * @cfg {String} colored determines whether use special progression coloring
     *      or the standard Ext.ProgressBar coloring for the bar (defaults to
     *      false)
     */
    colored: false,
    /**
     * @cfg {String} actionEvent Event to trigger actions, e.g. click, dblclick,
     *      mouseover (defaults to 'dblclick')
     */
    actionEvent: 'dblclick',
    init: function(grid) {
        this.grid = grid;
        this.view = grid.getView();

        if (this.editor && grid.isEditor) {
            var cfg = {
                scope: this
            };
            cfg[this.actionEvent] = this.onClick;
            grid.afterRender = grid.afterRender.createSequence(function() {
                this.view.mainBody.on(cfg);
            }, this);
        }
    },
    onClick: function(e, target) {
        var rowIndex = e.getTarget('.x-grid3-row').rowIndex;
        var colIndex = this.view.findCellIndex(target.parentNode.parentNode);

        var t = e.getTarget('.x-progress-text');
        if (t) {
            this.grid.startEditing(rowIndex, colIndex);
        }
    },
    renderer: function(v, p, record) {
        var style = '';
        var textClass = (v < (this.ceiling / 1.818)) ? 'x-progress-text-back' : 'x-progress-text-front' + (Ext.isIE6 ? '-ie6' : '');

        var value = v / this.ceiling * 100;
        value = value.toFixed(0);

        //ugly hack to deal with IE6 issue
        var text = String.format('</div><div class="x-progress-text {0}" style="width:100%;" id="{1}">{2}</div></div>',
                textClass, Ext.id(), value + this.textPst
                );
        text = (v < (this.ceiling / 1.031)) ? text.substring(0, text.length - 6) : text.substr(6);

        if (this.colored == true) {
            if (v <= this.ceiling && v > (this.ceiling * 0.66))
                style = '-green';
            if (v < (this.ceiling * 0.67) && v > (this.ceiling * 0.33))
                style = '-orange';
            if (v < (this.ceiling * 0.34))
                style = '-red';
        }

        p.css += ' x-grid3-progresscol';
        return String.format(
                '<div class="x-progress-wrap"><div class="x-progress-inner"><div class="x-progress-bar{0}" style="width:{1}%;">{2}</div>' +
                '</div>', style, value, text
                );
    }
});

//vim: ts=4:sw=4:nu:fdc=4:nospell
/*global Ext */
/**
 * @class Ext.ux.grid.RowActions
 * @extends Ext.util.Observable
 *
 * RowActions plugin for Ext grid. Contains renderer for icons and fires events when an icon is clicked.
 * CSS rules from Ext.ux.RowActions.css are mandatory
 *
 * Important general information: Actions are identified by iconCls. Wherever an <i>action</i>
 * is referenced (event argument, callback argument), the iconCls of clicked icon is used.
 * In other words, action identifier === iconCls.
 *
 * @author    Ing. Jozef Sakáloš
 * @copyright (c) 2008, by Ing. Jozef Sakáloš
 * @date      22. March 2008
 * @version   1.0
 * @revision  $Id: Ext.ux.grid.RowActions.js 747 2009-09-03 23:30:52Z jozo $
 *
 * @license Ext.ux.grid.RowActions is licensed under the terms of
 * the Open Source LGPL 3.0 license.  Commercial use is permitted to the extent
 * that the code/component(s) do NOT become part of another Open Source or Commercially
 * licensed development library or toolkit without explicit permission.
 * 
 * <p>License details: <a href="http://www.gnu.org/licenses/lgpl.html"
 * target="_blank">http://www.gnu.org/licenses/lgpl.html</a></p>
 *
 * @forum     29961
 * @demo      http://rowactions.extjs.eu
 * @download  
 * <ul>
 * <li><a href="http://rowactions.extjs.eu/rowactions.tar.bz2">rowactions.tar.bz2</a></li>
 * <li><a href="http://rowactions.extjs.eu/rowactions.tar.gz">rowactions.tar.gz</a></li>
 * <li><a href="http://rowactions.extjs.eu/rowactions.zip">rowactions.zip</a></li>
 * </ul>
 *
 * @donate
 * <form action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_blank">
 * <input type="hidden" name="cmd" value="_s-xclick">
 * <input type="hidden" name="hosted_button_id" value="3430419">
 * <input type="image" src="https://www.paypal.com/en_US/i/btn/x-click-butcc-donate.gif" 
 * border="0" name="submit" alt="PayPal - The safer, easier way to pay online.">
 * <img alt="" border="0" src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1" height="1">
 * </form>
 */

Ext.ns('Ext.ux.grid');

// add RegExp.escape if it has not been already added
if ('function' !== typeof RegExp.escape) {
    RegExp.escape = function(s) {
        if ('string' !== typeof s) {
            return s;
        }
        // Note: if pasting from forum, precede ]/\ with backslash manually
        return s.replace(/([.*+?\^=!:${}()|\[\]\/\\])/g, '\\$1');
    }; // eo function escape
}

/**
 * Creates new RowActions plugin
 * @constructor
 * @param {Object} config A config object
 */
Ext.ux.grid.RowActions = function(config) {
    Ext.apply(this, config);

    // {{{
    this.addEvents(
            /**
             * @event beforeaction
             * Fires before action event. Return false to cancel the subsequent action event.
             * @param {Ext.grid.GridPanel} grid
             * @param {Ext.data.Record} record Record corresponding to row clicked
             * @param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * @param {Integer} rowIndex Index of clicked grid row
             * @param {Integer} colIndex Index of clicked grid column that contains all action icons
             */
            'beforeaction'
            /**
             * @event action
             * Fires when icon is clicked
             * @param {Ext.grid.GridPanel} grid
             * @param {Ext.data.Record} record Record corresponding to row clicked
             * @param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * @param {Integer} rowIndex Index of clicked grid row
             * @param {Integer} colIndex Index of clicked grid column that contains all action icons
             */
            , 'action'
            /**
             * @event beforegroupaction
             * Fires before group action event. Return false to cancel the subsequent groupaction event.
             * @param {Ext.grid.GridPanel} grid
             * @param {Array} records Array of records in this group
             * @param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * @param {String} groupId Identifies the group clicked
             */
            , 'beforegroupaction'
            /**
             * @event groupaction
             * Fires when icon in a group header is clicked
             * @param {Ext.grid.GridPanel} grid
             * @param {Array} records Array of records in this group
             * @param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * @param {String} groupId Identifies the group clicked
             */
            , 'groupaction'
            );
    // }}}

    // call parent
    Ext.ux.grid.RowActions.superclass.constructor.call(this);
};

Ext.extend(Ext.ux.grid.RowActions, Ext.util.Observable, {
    // configuration options
    // {{{
    /**
     * @cfg {Array} actions Mandatory. Array of action configuration objects. The action
     * configuration object recognizes the following options:
     * <ul class="list">
     * <li style="list-style-position:outside">
     *   {Function} <b>callback</b> (optional). Function to call if the action icon is clicked.
     *   This function is called with same signature as action event and in its original scope.
     *   If you need to call it in different scope or with another signature use 
     *   createCallback or createDelegate functions. Works for statically defined actions. Use
     *   callbacks configuration options for store bound actions.
     * </li>
     * <li style="list-style-position:outside">
     *   {Function} <b>cb</b> Shortcut for callback.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>iconIndex</b> Optional, however either iconIndex or iconCls must be
     *   configured. Field name of the field of the grid store record that contains
     *   css class of the icon to show. If configured, shown icons can vary depending
     *   of the value of this field.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>iconCls</b> CSS class of the icon to show. It is ignored if iconIndex is
     *   configured. Use this if you want static icons that are not base on the values in the record.
     * </li>
     * <li style="list-style-position:outside">
     *   {Boolean} <b>hide</b> Optional. True to hide this action while still have a space in 
     *   the grid column allocated to it. IMO, it doesn't make too much sense, use hideIndex instead.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>hideIndex</b> Optional. Field name of the field of the grid store record that
     *   contains hide flag (falsie [null, '', 0, false, undefined] to show, anything else to hide).
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>qtipIndex</b> Optional. Field name of the field of the grid store record that 
     *   contains tooltip text. If configured, the tooltip texts are taken from the store.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>tooltip</b> Optional. Tooltip text to use as icon tooltip. It is ignored if 
     *   qtipIndex is configured. Use this if you want static tooltips that are not taken from the store.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>qtip</b> Synonym for tooltip
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>textIndex</b> Optional. Field name of the field of the grids store record
     *   that contains text to display on the right side of the icon. If configured, the text
     *   shown is taken from record.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>text</b> Optional. Text to display on the right side of the icon. Use this
     *   if you want static text that are not taken from record. Ignored if textIndex is set.
     * </li>
     * <li style="list-style-position:outside">
     *   {String} <b>style</b> Optional. Style to apply to action icon container.
     * </li>
     * </ul>
     */

    /**
     * @cfg {String} actionEvent Event to trigger actions, e.g. click, dblclick, mouseover (defaults to 'click')
     */
    actionEvent: 'click'
            /**
             * @cfg {Boolean} autoWidth true to calculate field width for iconic actions only (defaults to true).
             * If true, the width is calculated as {@link #widthSlope} * number of actions + {@link #widthIntercept}.
             */
    , autoWidth: true

            /**
             * @cfg {String} dataIndex - Do not touch!
             * @private
             */
    , dataIndex: ''

            /**
             * @cfg {Boolean} editable - Do not touch!
             * Must be false to prevent errors in editable grids
             */
    , editable: false

            /**
             * @cfg {Array} groupActions Array of action to use for group headers of grouping grids.
             * These actions support static icons, texts and tooltips same way as {@link #actions}. There is one
             * more action config option recognized:
             * <ul class="list">
             * <li style="list-style-position:outside">
             *   {String} <b>align</b> Set it to 'left' to place action icon next to the group header text.
             *   (defaults to undefined = icons are placed at the right side of the group header.
             * </li>
             * </ul>
             */

            /**
             * @cfg {Object} callbacks iconCls keyed object that contains callback functions. For example:
             * <pre>
             * callbacks:{
             * &nbsp;    'icon-open':function(...) {...}
             * &nbsp;   ,'icon-save':function(...) {...}
             * }
             * </pre>
             */

            /**
             * @cfg {String} header Actions column header
             */
    , header: ''

            /**
             * @cfg {Boolean} isColumn
             * Tell ColumnModel that we are column. Do not touch!
             * @private
             */
    , isColumn: true

            /**
             * @cfg {Boolean} keepSelection
             * Set it to true if you do not want action clicks to affect selected row(s) (defaults to false).
             * By default, when user clicks an action icon the clicked row is selected and the action events are fired.
             * If this option is true then the current selection is not affected, only the action events are fired.
             */
    , keepSelection: false

            /**
             * @cfg {Boolean} menuDisabled No sense to display header menu for this column
             * @private
             */
    , menuDisabled: true

            /**
             * @cfg {Boolean} sortable Usually it has no sense to sort by this column
             * @private
             */
    , sortable: false

            /**
             * @cfg {String} tplGroup Template for group actions
             * @private
             */
    , tplGroup:
            '<tpl for="actions">'
            + '<div class="ux-grow-action-item<tpl if="\'right\'===align"> ux-action-right</tpl> '
            + '{cls}" style="{style}" qtip="{qtip}">{text}</div>'
            + '</tpl>'

            /**
             * @cfg {String} tplRow Template for row actions
             * @private
             */
    , tplRow:
            '<div class="ux-row-action">'
            + '<tpl for="actions">'
            + '<div class="ux-row-action-item {cls} <tpl if="text">'
            + 'ux-row-action-text</tpl>" style="{hide}{style}" qtip="{qtip}">'
            + '<tpl if="text"><span qtip="{qtip}">{text}</span></tpl></div>'
            + '</tpl>'
            + '</div>'

            /**
             * @cfg {String} hideMode How to hide hidden icons. Valid values are: 'visibility' and 'display' 
             * (defaluts to 'visibility'). If the mode is visibility the hidden icon is not visible but there
             * is still blank space occupied by the icon. In display mode, the visible icons are shifted taking
             * the space of the hidden icon.
             */
    , hideMode: 'visibility'

            /**
             * @cfg {Number} widthIntercept Constant used for auto-width calculation (defaults to 4).
             * See {@link #autoWidth} for explanation.
             */
    , widthIntercept: 4

            /**
             * @cfg {Number} widthSlope Constant used for auto-width calculation (defaults to 21).
             * See {@link #autoWidth} for explanation.
             */
    , widthSlope: 21
            // }}}

            // methods
            // {{{
            /**
             * Init function
             * @param {Ext.grid.GridPanel} grid Grid this plugin is in
             */
    , init: function(grid) {
        this.grid = grid;

        // the actions column must have an id for Ext 3.x
        this.id = this.id || Ext.id();

        // for Ext 3.x compatibility
        var lookup = grid.getColumnModel().lookup;
        delete(lookup[undefined]);
        lookup[this.id] = this;

        // {{{
        // setup template
        if (!this.tpl) {
            this.tpl = this.processActions(this.actions);

        } // eo template setup
        // }}}

        // calculate width
        if (this.autoWidth) {
            this.width = this.widthSlope * this.actions.length + this.widthIntercept;
            this.fixed = true;
        }

        // body click handler
        var view = grid.getView();
        var cfg = {scope: this};
        cfg[this.actionEvent] = this.onClick;
        grid.afterRender = grid.afterRender.createSequence(function() {
            view.mainBody.on(cfg);
            grid.on('destroy', this.purgeListeners, this);
        }, this);

        // setup renderer
        if (!this.renderer) {
            this.renderer = function(value, cell, record, row, col, store) {
                cell.css += (cell.css ? ' ' : '') + 'ux-row-action-cell';
                return this.tpl.apply(this.getData(value, cell, record, row, col, store));
            }.createDelegate(this);
        }

        // actions in grouping grids support
        if (view.groupTextTpl && this.groupActions) {
            view.interceptMouse = view.interceptMouse.createInterceptor(function(e) {
                if (e.getTarget('.ux-grow-action-item')) {
                    return false;
                }
            });
            view.groupTextTpl =
                    '<div class="ux-grow-action-text">' + view.groupTextTpl + '</div>'
                    + this.processActions(this.groupActions, this.tplGroup).apply()
                    ;
        }

        // cancel click
        if (true === this.keepSelection) {
            grid.processEvent = grid.processEvent.createInterceptor(function(name, e) {
                if ('mousedown' === name) {
                    return !this.getAction(e);
                }
            }, this);
        }

    } // eo function init
    // }}}
    // {{{
    /**
     * Returns data to apply to template. Override this if needed.
     * @param {Mixed} value 
     * @param {Object} cell object to set some attributes of the grid cell
     * @param {Ext.data.Record} record from which the data is extracted
     * @param {Number} row row index
     * @param {Number} col col index
     * @param {Ext.data.Store} store object from which the record is extracted
     * @return {Object} data to apply to template
     */
    , getData: function(value, cell, record, row, col, store) {
        return record.data || {};
    } // eo function getData
    // }}}
    // {{{
    /**
     * Processes actions configs and returns template.
     * @param {Array} actions
     * @param {String} template Optional. Template to use for one action item.
     * @return {String}
     * @private
     */
    , processActions: function(actions, template) {
        var acts = [];

        // actions loop
        Ext.each(actions, function(a, i) {
            // save callback
            if (a.iconCls && 'function' === typeof (a.callback || a.cb)) {
                this.callbacks = this.callbacks || {};
                this.callbacks[a.iconCls] = a.callback || a.cb;
            }

            // data for intermediate template
            var o = {
                cls: a.iconIndex ? '{' + a.iconIndex + '}' : (a.iconCls ? a.iconCls : '')
                , qtip: a.qtipIndex ? '{' + a.qtipIndex + '}' : (a.tooltip || a.qtip ? a.tooltip || a.qtip : '')
                , text: a.textIndex ? '{' + a.textIndex + '}' : (a.text ? a.text : '')
                , hide: a.hideIndex
                        ? '<tpl if="' + a.hideIndex + '">'
                        + ('display' === this.hideMode ? 'display:none' : 'visibility:hidden') + ';</tpl>'
                        : (a.hide ? ('display' === this.hideMode ? 'display:none' : 'visibility:hidden;') : '')
                , align: a.align || 'right'
                , style: a.style ? a.style : ''
            };
            acts.push(o);

        }, this); // eo actions loop

        var xt = new Ext.XTemplate(template || this.tplRow);
        return new Ext.XTemplate(xt.apply({actions: acts}));

    } // eo function processActions
    // }}}
    , getAction: function(e) {
        var action = false;
        var t = e.getTarget('.ux-row-action-item');
        if (t) {
            action = t.className.replace(/ux-row-action-item /, '');
            if (action) {
                action = action.replace(/ ux-row-action-text/, '');
                action = action.trim();
            }
        }
        return action;
    } // eo function getAction
    // {{{
    /**
     * Grid body actionEvent event handler
     * @private
     */
    , onClick: function(e, target) {

        var view = this.grid.getView();

        // handle row action click
        var row = e.getTarget('.x-grid3-row');
        var col = view.findCellIndex(target.parentNode.parentNode);
        var action = this.getAction(e);

//		var t = e.getTarget('.ux-row-action-item');
//		if(t) {
//			action = this.getAction(t);
//			action = t.className.replace(/ux-row-action-item /, '');
//			if(action) {
//				action = action.replace(/ ux-row-action-text/, '');
//				action = action.trim();
//			}
//		}
        if (false !== row && false !== col && false !== action) {
            var record = this.grid.store.getAt(row.rowIndex);

            // call callback if any
            if (this.callbacks && 'function' === typeof this.callbacks[action]) {
                this.callbacks[action](this.grid, record, action, row.rowIndex, col);
            }

            // fire events
            if (true !== this.eventsSuspended && false === this.fireEvent('beforeaction', this.grid, record, action, row.rowIndex, col)) {
                return;
            }
            else if (true !== this.eventsSuspended) {
                this.fireEvent('action', this.grid, record, action, row.rowIndex, col);
            }

        }

        // handle group action click
        t = e.getTarget('.ux-grow-action-item');
        if (t) {
            // get groupId
            var group = view.findGroup(target);
            var groupId = group ? group.id.replace(/ext-gen[0-9]+-gp-/, '') : null;

            // get matching records
            var records;
            if (groupId) {
                var re = new RegExp(RegExp.escape(groupId));
                records = this.grid.store.queryBy(function(r) {
                    return r._groupId.match(re);
                });
                records = records ? records.items : [];
            }
            action = t.className.replace(/ux-grow-action-item (ux-action-right )*/, '');

            // call callback if any
            if ('function' === typeof this.callbacks[action]) {
                this.callbacks[action](this.grid, records, action, groupId);
            }

            // fire events
            if (true !== this.eventsSuspended && false === this.fireEvent('beforegroupaction', this.grid, records, action, groupId)) {
                return false;
            }
            this.fireEvent('groupaction', this.grid, records, action, groupId);
        }
    } // eo function onClick
    // }}}

});

// registre xtype
Ext.reg('rowactions', Ext.ux.grid.RowActions);

// eof



/**
 * Ext.ux.form.LovCombo, List of Values Combo
 *
 * @author    Ing. Jozef Sakáloš
 * @copyright (c) 2008, by Ing. Jozef Sakáloš
 * @date      16. April 2008
 * @version   $Id: Ext.ux.form.LovCombo.js 285 2008-06-06 09:22:20Z jozo $
 *
 * @license Ext.ux.form.LovCombo.js is licensed under the terms of the Open Source
 * LGPL 3.0 license. Commercial use is permitted to the extent that the 
 * code/component(s) do NOT become part of another Open Source or Commercially
 * licensed development library or toolkit without explicit permission.
 * 
 * License details: http://www.gnu.org/licenses/lgpl.html
 */

/*global Ext */

// add RegExp.escape if it has not been already added
if ('function' !== typeof RegExp.escape) {
    RegExp.escape = function(s) {
        if ('string' !== typeof s) {
            return s;
        }
        // Note: if pasting from forum, precede ]/\ with backslash manually
        return s.replace(/([.*+?^=!:${}()|[\]\/\\])/g, '\\$1');
    }; // eo function escape
}

// create namespace
Ext.ns('Ext.ux.form');

/**
 *
 * @class Ext.ux.form.LovCombo
 * @extends Ext.form.ComboBox
 */
Ext.ux.form.LovCombo = Ext.extend(Ext.form.ComboBox, {
    // {{{
    // configuration options
    /**
     * @cfg {String} checkField name of field used to store checked state.
     * It is automatically added to existing fields.
     * Change it only if it collides with your normal field.
     */
    checkField: 'checked'

            /**
             * @cfg {String} separator separator to use between values and texts
             */
    , separator: ','

            /**
             * @cfg {String/Array} tpl Template for items. 
             * Change it only if you know what you are doing.
             */
            // }}}
            // {{{
    , initComponent: function() {

        // template with checkbox
        if (!this.tpl) {
            this.tpl =
                    '<tpl for=".">'
                    + '<div class="x-combo-list-item">'
                    + '<img src="' + Ext.BLANK_IMAGE_URL + '" '
                    + 'class="ux-lovcombo-icon ux-lovcombo-icon-'
                    + '{[values.' + this.checkField + '?"checked":"unchecked"' + ']}">'
                    + '<div class="ux-lovcombo-item-text">{' + (this.displayField || 'text') + '}</div>'
                    + '</div>'
                    + '</tpl>'
                    ;
        }

        // call parent
        Ext.ux.form.LovCombo.superclass.initComponent.apply(this, arguments);

        // install internal event handlers
        this.on({
            scope: this
            , beforequery: this.onBeforeQuery
            , blur: this.onRealBlur
        });

        // remove selection from input field
        this.onLoad = this.onLoad.createSequence(function() {
            if (this.el) {
                var v = this.el.dom.value;
                this.el.dom.value = '';
                this.el.dom.value = v;
            }
        });

    } // e/o function initComponent
    // }}}
    // {{{
    /**
     * Disables default tab key bahavior
     * @private
     */
    , initEvents: function() {
        Ext.ux.form.LovCombo.superclass.initEvents.apply(this, arguments);

        // disable default tab handling - does no good
        this.keyNav.tab = false;

    } // eo function initEvents
    // }}}
    // {{{
    /**
     * clears value
     */
    , clearValue: function() {
        this.value = '';
        this.setRawValue(this.value);
        this.store.clearFilter();
        this.store.each(function(r) {
            r.set(this.checkField, false);
        }, this);
        if (this.hiddenField) {
            this.hiddenField.value = '';
        }
        this.applyEmptyText();
    } // eo function clearValue
    // }}}
    // {{{
    /**
     * @return {String} separator (plus space) separated list of selected displayFields
     * @private
     */
    , getCheckedDisplay: function() {
        var re = new RegExp(this.separator, "g");
        return this.getCheckedValue(this.displayField).replace(re, this.separator + ' ');
    } // eo function getCheckedDisplay
    // }}}
    // {{{
    /**
     * @return {String} separator separated list of selected valueFields
     * @private
     */
    , getCheckedValue: function(field) {
        field = field || this.valueField;
        var c = [];

        // store may be filtered so get all records
        var snapshot = this.store.snapshot || this.store.data;

        snapshot.each(function(r) {
            if (r.get(this.checkField)) {
                c.push(r.get(field));
            }
        }, this);

        return c.join(this.separator);
    } // eo function getCheckedValue
    // }}}
    // {{{
    /**
     * beforequery event handler - handles multiple selections
     * @param {Object} qe query event
     * @private
     */
    , onBeforeQuery: function(qe) {
        qe.query = qe.query.replace(new RegExp(this.getCheckedDisplay() + '[ ' + this.separator + ']*'), '');
    } // eo function onBeforeQuery
    // }}}
    // {{{
    /**
     * blur event handler - runs only when real blur event is fired
     */
    , onRealBlur: function() {
        this.list.hide();
        var rv = this.getRawValue();
        var rva = rv.split(new RegExp(RegExp.escape(this.separator) + ' *'));
        var va = [];
        var snapshot = this.store.snapshot || this.store.data;

        // iterate through raw values and records and check/uncheck items
        Ext.each(rva, function(v) {
            snapshot.each(function(r) {
                if (v === r.get(this.displayField)) {
                    va.push(r.get(this.valueField));
                }
            }, this);
        }, this);
        this.setValue(va.join(this.separator));
        this.store.clearFilter();
    } // eo function onRealBlur
    // }}}
    // {{{
    /**
     * Combo's onSelect override
     * @private
     * @param {Ext.data.Record} record record that has been selected in the list
     * @param {Number} index index of selected (clicked) record
     */
    , onSelect: function(record, index) {
        if (this.fireEvent('beforeselect', this, record, index) !== false) {

            // toggle checked field
            record.set(this.checkField, !record.get(this.checkField));

            // display full list
            if (this.store.isFiltered()) {
                this.doQuery(this.allQuery);
            }

            // set (update) value and fire event
            this.setValue(this.getCheckedValue());
            this.fireEvent('select', this, record, index);
        }
    } // eo function onSelect
    // }}}
    // {{{
    /**
     * Sets the value of the LovCombo
     * @param {Mixed} v value
     */
    , setValue: function(v) {
        if (v) {
            v = '' + v;
            if (this.valueField) {
                this.store.clearFilter();
                this.store.each(function(r) {
                    var checked = !(!v.match(
                            '(^|' + this.separator + ')' + RegExp.escape(r.get(this.valueField))
                            + '(' + this.separator + '|$)'))
                            ;

                    r.set(this.checkField, checked);
                }, this);
                this.value = this.getCheckedValue();
                this.setRawValue(this.getCheckedDisplay());
                if (this.hiddenField) {
                    this.hiddenField.value = this.value;
                }
            }
            else {
                this.value = v;
                this.setRawValue(v);
                if (this.hiddenField) {
                    this.hiddenField.value = v;
                }
            }
            if (this.el) {
                this.el.removeClass(this.emptyClass);
            }
        }
        else {
            this.clearValue();
        }
    } // eo function setValue
    // }}}
    // {{{
    /**
     * Selects all items
     */
    , selectAll: function() {
        this.store.each(function(record) {
            // toggle checked field
            record.set(this.checkField, true);
        }, this);

        //display full list
        this.doQuery(this.allQuery);
        this.setValue(this.getCheckedValue());
    } // eo full selectAll
    // }}}
    // {{{
    /**
     * Deselects all items. Synonym for clearValue
     */
    , deselectAll: function() {
        this.clearValue();
    } // eo full deselectAll 
    // }}}

}); // eo extend

// register xtype
Ext.reg('lovcombo', Ext.ux.form.LovCombo);
