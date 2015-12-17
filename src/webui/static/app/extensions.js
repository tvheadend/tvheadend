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
 * 01/08/2014: tvh_renderer fcn added by Jaroslav Kysela
 * 09/06/2015: update timeout code added by Jaroslav Kysela
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
     * @cfg {Integer} update timeout in milliseconds (defaults to 0 - inactive)
     */
    timeout : 0,
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
    tvh_renderer: function (v, p, record) {
      return v;
    },
    renderer: function(v, p, record) {
        var ov = v;
        v = this.tvh_renderer(v, p, record);
        if (typeof v === "string")
          return v; // custom string

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

        var res = String.format(
                '<div class="x-progress-wrap"><div class="x-progress-inner"><div class="x-progress-bar{0}" style="width:{1}%;">{2}</div>' +
                '</div>', style, value, text
                );
        if (!this.timerflag) {
            p.css += ' x-grid3-progresscol';
            if (this.timeout) {
              var tid = Ext.id();
              res = '<div id="' + tid + '">' + res + '</div>';
              setInterval(this.runTimer, this.timeout, this, ov, p, record, tid);
            }
        }
        return res;
    },
    runTimer: function(obj, v, p, record, tid) {
        var dom = document.getElementById(tid);
        if (dom) {
           obj.timerflag = true;
           var res = obj.renderer(v, p, record, tid);
           obj.timerflag = false;
           dom.innerHTML = res;
        }
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
             * event beforeaction
             * Fires before action event. Return false to cancel the subsequent action event.
             * param {Ext.grid.GridPanel} grid
             * param {Ext.data.Record} record Record corresponding to row clicked
             * param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * param {Integer} rowIndex Index of clicked grid row
             * param {Integer} colIndex Index of clicked grid column that contains all action icons
             */
            'beforeaction'
            /**
             * event action
             * Fires when icon is clicked
             * param {Ext.grid.GridPanel} grid
             * param {Ext.data.Record} record Record corresponding to row clicked
             * param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * param {Integer} rowIndex Index of clicked grid row
             * param {Integer} colIndex Index of clicked grid column that contains all action icons
             */
            , 'action'
            /**
             * event beforegroupaction
             * Fires before group action event. Return false to cancel the subsequent groupaction event.
             * param {Ext.grid.GridPanel} grid
             * param {Array} records Array of records in this group
             * param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * param {String} groupId Identifies the group clicked
             */
            , 'beforegroupaction'
            /**
             * event groupaction
             * Fires when icon in a group header is clicked
             * param {Ext.grid.GridPanel} grid
             * param {Array} records Array of records in this group
             * param {String} action Identifies the action icon clicked. Equals to icon css class name.
             * param {String} groupId Identifies the group clicked
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

/**
 * @class Ext.ux.form.TwinDateTimeField
 * @extends Ext.form.Field
 *
 * DateTime field, combination of DateField and TimeField
 *
 * @author Ing. Jozef Sakáloš
 * @copyright (c) 2008, Ing. Jozef Sakáloš
 * @version 2.0
 * @revision $Id: Ext.ux.form.TwinDateTimeField.js 813 2010-01-29 23:32:36Z jozo $
 *
 * @license Ext.ux.form.TwinDateTimeField is licensed under the terms of the Open Source
 *          LGPL 3.0 license. Commercial use is permitted to the extent that the
 *          code/component(s) do NOT become part of another Open Source or
 *          Commercially licensed development library or toolkit without
 *          explicit permission.
 *
 * <p>
 * License details: <a href="http://www.gnu.org/licenses/lgpl.html"
 * target="_blank">http://www.gnu.org/licenses/lgpl.html</a>
 * </p>
 */

Ext.ns('Ext.ux.form');

// register xtype
//Ext.reg('twindatetime', Ext.ux.form.TwinDateTimeField);
//Ext.namespace('Ext.ux.form');
Ext.ux.form.TwinDateField = Ext.extend(Ext.form.DateField, {
  getTrigger : Ext.form.TwinTriggerField.prototype.getTrigger,
  initTrigger : Ext.form.TwinTriggerField.prototype.initTrigger,
  initComponent : Ext.form.TwinTriggerField.prototype.initComponent,
  trigger2Class : 'x-form-date-trigger',
  trigger1Class : 'x-form-clear-trigger',
  hideTrigger1 : true,
  submitOnSelect : true,
  submitOnClear : true,
  allowClear : true,
  defaultValue : null,

  onSelect : Ext.form.DateField.prototype.onSelect.createSequence(function(v) {
    if (this.value && this.ownerCt && this.ownerCt.buttons && this.submitOnSelect) {
      this.ownerCt.buttons[0].handler.call(this.ownerCt);
    }
  }),

  onRender : Ext.form.DateField.prototype.onRender.createSequence(function(v) {
    this.getTrigger(0).hide();
  }),

  setValue : Ext.form.DateField.prototype.setValue.createSequence(function(v) {
    if (!this.triggers)
      return;
    if (v !== null && v != '') {
      if (this.allowClear)
        this.getTrigger(0).show();
    } else {
      this.getTrigger(0).hide();
    }
  }),

  reset : Ext.form.DateField.prototype.reset.createSequence(function() {
    this.originalValue = this.defaultValue;
    this.setValue(this.defaultValue);
    if (this.allowClear)
      this.getTrigger(0).hide();
  }),

  onTrigger2Click : function() {
    if (!this.readOnly)
      this.onTriggerClick();
  },

  onTrigger1Click : function() {
    if (!this.disabled && !this.readOnly) {
      this.clearValue();
      this.getTrigger(0).hide();
      if (this.ownerCt && this.ownerCt.buttons && this.submitOnClear) {
        this.ownerCt.buttons[0].handler.call(this.ownerCt);
      }
      this.fireEvent('clear', this);
      this.onFocus();
    }
  },

  /**
   * Clears any text/value currently set in the field
   */
  clearValue : function() {
    if (this.hiddenField) {
      this.hiddenField.value = '';
    }
    this.setRawValue('');
    this.lastSelectionText = '';
    this.applyEmptyText();
    this.value = '';
  }
});
//Ext.ComponentMgr.registerType('twindatefield', Ext.ux.form.TwinDateField);


/**
 * Creates new DateTime
 *
 * @constructor
 * @param {Object}
 *          config A config object
 */
Ext.ux.form.TwinDateTimeField = Ext.extend(Ext.form.Field, {
  /**
   * @cfg {Function} dateValidator A custom validation function to be called
   *      during date field validation (defaults to null)
   */
  dateValidator : null,

  /**
   * @cfg {String/Object} defaultAutoCreate DomHelper element spec Let
   *      superclass to create hidden field instead of textbox. Hidden will be
   *      submittend to server
   */
  defaultAutoCreate : {
    tag : 'input',
    type : 'hidden'
  },

  /**
   * @cfg {String} dtSeparator Date - Time separator. Used to split date and
   *      time (defaults to ' ' (space))
   */
  dtSeparator : ' ',

  /**
   * @cfg {String} hiddenFormat Format of datetime used to store value in hidden
   *      field and submitted to server (defaults to 'Y-m-d H:i:s' that is mysql
   *      format)
   */
  hiddenFormat : 'Y-m-d H:i:s',

  /**
   * @cfg {Boolean} otherToNow Set other field to now() if not explicly filled
   *      in (defaults to true)
   */
  otherToNow : true,

  /**
   * @cfg {Boolean} emptyToNow Set field value to now on attempt to set empty
   *      value. If it is true then setValue() sets value of field to current
   *      date and time (defaults to false)
   */
  /**
   * @cfg {String} timePosition Where the time field should be rendered. 'right'
   *      is suitable for forms and 'below' is suitable if the field is used as
   *      the grid editor (defaults to 'right')
   */
  timePosition : 'right', // valid values:'below', 'right'

  /**
   * @cfg {Function} timeValidator A custom validation function to be called
   *      during time field validation (defaults to null)
   */
  timeValidator : null,

  /**
   * @cfg {Number} timeWidth Width of time field in pixels (defaults to 100)
   */
  timeWidth : 100,

  /**
   * @cfg {String} dateFormat Format of DateField. Can be localized. (defaults
   *      to 'm/y/d')
   */
  dateFormat : 'm/d/Y',

  /**
   * @cfg {String} timeFormat Format of TimeField. Can be localized. (defaults
   *      to 'g:i A')
   */
  timeFormat : 'g:i A',

  /**
   * @cfg {Object} dateConfig Config for DateField constructor.
   */
  /**
   * @cfg {Object} timeConfig Config for TimeField constructor.
   */

  /**
   * @private creates DateField and TimeField and installs the necessary event
   *          handlers
   */
  initComponent : function() {
    // call parent initComponent
    Ext.ux.form.TwinDateTimeField.superclass.initComponent.call(this);

    // create DateField
    var dateConfig = Ext.apply({}, {
      id : this.id + '-date',
      format : this.dateFormat || Ext.ux.form.TwinDateField.prototype.format,
      width : this.timeWidth,
      selectOnFocus : this.selectOnFocus,
      validator : this.dateValidator,
      listeners : {
        blur : {
          scope : this,
          fn : this.onBlur
        },
        focus : {
          scope : this,
          fn : this.onFocus
        }
      }
    }, this.dateConfig);
    this.df = new Ext.ux.form.TwinDateField(dateConfig);
    this.df.ownerCt = this;
    delete(this.dateFormat);

    // create TimeField
    var timeConfig = Ext.apply({}, {
      id : this.id + '-time',
      format : this.timeFormat || Ext.form.TimeField.prototype.format,
      width : this.timeWidth,
      selectOnFocus : this.selectOnFocus,
      validator : this.timeValidator,
      listeners : {
        blur : {
          scope : this,
          fn : this.onBlur
        },
        focus : {
          scope : this,
          fn : this.onFocus
        }
      }
    }, this.timeConfig);
    this.tf = new Ext.form.TimeField(timeConfig);
    this.tf.ownerCt = this;
    delete(this.timeFormat);

    // relay events
    this.relayEvents(this.df, ['focus', 'specialkey', 'invalid', 'valid']);
    this.relayEvents(this.tf, ['focus', 'specialkey', 'invalid', 'valid']);

    this.on('specialkey', this.onSpecialKey, this);
  },

  /**
   * @private Renders underlying DateField and TimeField and provides a
   *          workaround for side error icon bug
   */
  onRender : function(ct, position) {
    // don't run more than once
    if (this.isRendered) {
      return;
    }

    // render underlying hidden field
    Ext.ux.form.TwinDateTimeField.superclass.onRender.call(this, ct, position);

    // render DateField and TimeField
    // create bounding table
    var t;
    if ('below' === this.timePosition || 'bellow' === this.timePosition) {
      t = Ext.DomHelper.append(ct, {
        tag : 'table',
        style : 'border-collapse:collapse',
        children : [{
          tag : 'tr',
          children : [{
            tag : 'td',
            style : 'padding-bottom:1px',
            cls : 'ux-datetime-date'
          }]
        }, {
          tag : 'tr',
          children : [{
            tag : 'td',
            cls : 'ux-datetime-time'
          }]
        }]
      }, true);
    } else {
      t = Ext.DomHelper.append(ct, {
        tag : 'table',
        style : 'border-collapse:collapse',
        children : [{
          tag : 'tr',
          children : [{
            tag : 'td',
            style : 'padding-right:4px',
            cls : 'ux-datetime-date'
          }, {
            tag : 'td',
            cls : 'ux-datetime-time'
          }]
        }]
      }, true);
    }

    this.tableEl = t;
    this.wrap = t.wrap({
      cls : 'x-form-field-wrap'
    });
    // this.wrap = t.wrap();
    this.wrap.on("mousedown", this.onMouseDown, this, {
      delay : 10
    });

    // render DateField & TimeField
    this.df.render(t.child('td.ux-datetime-date'));
    this.tf.render(t.child('td.ux-datetime-time'));

    // workaround for IE trigger misalignment bug
    // see http://extjs.com/forum/showthread.php?p=341075#post341075
    // if(Ext.isIE && Ext.isStrict) {
    // t.select('input').applyStyles({top:0});
    // }

    this.df.el.swallowEvent(['keydown', 'keypress']);
    this.tf.el.swallowEvent(['keydown', 'keypress']);

    // create icon for side invalid errorIcon
    if ('side' === this.msgTarget) {
      var elp = this.el.findParent('.x-form-element', 10, true);
      if (elp) {
        this.errorIcon = elp.createChild({
          cls : 'x-form-invalid-icon'
        });
      }

      var o = {
        errorIcon : this.errorIcon,
        msgTarget : 'side',
        alignErrorIcon : this.alignErrorIcon.createDelegate(this)
      };
      Ext.apply(this.df, o);
      Ext.apply(this.tf, o);
      // this.df.errorIcon = this.errorIcon;
      // this.tf.errorIcon = this.errorIcon;
    }

    // setup name for submit
    this.el.dom.name = this.hiddenName || this.name || this.id;

    // prevent helper fields from being submitted
    this.df.el.dom.removeAttribute("name");
    this.tf.el.dom.removeAttribute("name");

    // we're rendered flag
    this.isRendered = true;

    // update hidden field
    this.updateHidden();

  },

  /**
   * @private
   */
  adjustSize : Ext.BoxComponent.prototype.adjustSize,

  /**
   * @private
   */
  alignErrorIcon : function() {
    this.errorIcon.alignTo(this.tableEl, 'tl-tr', [2, 0]);
  },

  /**
   * @private initializes internal dateValue
   */
  initDateValue : function() {
    this.dateValue = this.otherToNow ? new Date() : new Date(1970, 0, 1, 0, 0, 0);
  },

  /**
   * Calls clearInvalid on the DateField and TimeField
   */
  clearInvalid : function() {
    this.df.clearInvalid();
    this.tf.clearInvalid();
  },

  /**
   * Calls markInvalid on both DateField and TimeField
   *
   * @param {String}
   *          msg Invalid message to display
   */
  markInvalid : function(msg) {
    this.df.markInvalid(msg);
    this.tf.markInvalid(msg);
  },

  /**
   * @private called from Component::destroy. Destroys all elements and removes
   *          all listeners we've created.
   */
  beforeDestroy : function() {
    if (this.isRendered) {
      // this.removeAllListeners();
      this.wrap.removeAllListeners();
      this.wrap.remove();
      this.tableEl.remove();
      this.df.destroy();
      this.tf.destroy();
    }
  },

  /**
   * Disable this component.
   *
   * @return {Ext.Component} this
   */
  disable : function() {
    if (this.isRendered) {
      this.df.disabled = this.disabled;
      this.df.onDisable();
      this.tf.onDisable();
    }
    this.disabled = true;
    this.df.disabled = true;
    this.tf.disabled = true;
    this.fireEvent("disable", this);
    return this;
  },

  /**
   * Enable this component.
   *
   * @return {Ext.Component} this
   */
  enable : function() {
    if (this.rendered) {
      this.df.onEnable();
      this.tf.onEnable();
    }
    this.disabled = false;
    this.df.disabled = false;
    this.tf.disabled = false;
    this.fireEvent("enable", this);
    return this;
  },

  /**
   * @private Focus date filed
   */
  focus : function() {
    this.df.focus();
  },

  /**
   * @private
   */
  getPositionEl : function() {
    return this.wrap;
  },

  /**
   * @private
   */
  getResizeEl : function() {
    return this.wrap;
  },

  /**
   * @return {Date/String} Returns value of this field
   */
  getValue : function() {
    // create new instance of date
    return this.dateValue ? parseInt(this.dateValue.getTime() / 1000) : '';
  },

  /**
   * @return {Boolean} true = valid, false = invalid
   * @private Calls isValid methods of underlying DateField and TimeField and
   *          returns the result
   */
  isValid : function() {
    return this.df.isValid() && this.tf.isValid();
  },

  /**
   * Returns true if this component is visible
   *
   * @return {boolean}
   */
  isVisible : function() {
    return this.df.rendered && this.df.getActionEl().isVisible();
  },

  /**
   * @private Handles blur event
   */
  onBlur : function(f) {
    // called by both DateField and TimeField blur events

    // revert focus to previous field if clicked in between
    if (this.wrapClick) {
      f.focus();
      this.wrapClick = false;
    }

    // update underlying value
    if (f === this.df) {
      this.updateDate();
    } else {
      this.updateTime();
    }
    this.updateHidden();

    this.validate();

// fire events later
    (function() {
      if (!this.df.hasFocus && !this.tf.hasFocus) {
        var v = this.getValue();
        if (String(v) !== String(this.startValue)) {
          this.fireEvent("change", this, v, this.startValue);
        }
        this.hasFocus = false;
        this.fireEvent('blur', this);
      }
    }).defer(100, this);
  },

  /**
   * @private Handles focus event
   */
  onFocus : function() {
    if (!this.hasFocus) {
      this.hasFocus = true;
      this.startValue = this.getValue();
      this.fireEvent("focus", this);
    }
  },

  /**
   * @private Just to prevent blur event when clicked in the middle of fields
   */
  onMouseDown : function(e) {
    if (!this.disabled) {
      this.wrapClick = 'td' === e.target.nodeName.toLowerCase();
    }
  },

  /**
   * @private Handles Tab and Shift-Tab events
   */
  onSpecialKey : function(t, e) {
    var key = e.getKey();
    if (key === e.TAB) {
      if (t === this.df && !e.shiftKey) {
        e.stopEvent();
        this.tf.focus();
      }
      if (t === this.tf && e.shiftKey) {
        e.stopEvent();
        this.df.focus();
      }
      this.updateValue();
    }
    // otherwise it misbehaves in editor grid
    if (key === e.ENTER) {
      this.updateValue();
    }
  },


  /**
   * Resets the current field value to the originally loaded value and clears
   * any validation messages. See Ext.form.BasicForm.trackResetOnLoad
   */
  reset : function() {
    this.df.setValue(this.originalValue);
    this.tf.setValue(this.originalValue);
  },

  /**
   * @private Sets the value of DateField
   */
  setDate : function(date) {
    this.df.setValue(date);
  },

  /**
   * @private Sets the value of TimeField
   */
  setTime : function(date) {
    this.tf.setValue(date);
  },

  /**
   * @private Sets correct sizes of underlying DateField and TimeField With
   *          workarounds for IE bugs
   */
  setSize : function(w, h) {
    if (!w) {
      return;
    }
    if ('below' === this.timePosition) {
      this.df.setSize(w, h);
      this.tf.setSize(w, h);
      if (Ext.isIE) {
        this.df.el.up('td').setWidth(w);
        this.tf.el.up('td').setWidth(w);
      }
    } else {
      this.df.setSize(w - this.timeWidth - 4, h);
      this.tf.setSize(this.timeWidth, h);

      if (Ext.isIE) {
        this.df.el.up('td').setWidth(w - this.timeWidth - 4);
        this.tf.el.up('td').setWidth(this.timeWidth);
      }
    }
  },

  /**
   * @param {Mixed}
   *          val Value to set Sets the value of this field
   */
  setValue : function(val) {
    if (!val && true === this.emptyToNow) {
      this.setValue(new Date());
      return;
    } else if (!val) {
      this.setDate('');
      this.setTime('');
      this.updateValue();
      return;
    }
    if ('number' === typeof val) {
      val = new Date(val * 1000);
    } else if ('string' === typeof val && this.hiddenFormat) {
      val = Date.parseDate(val, this.hiddenFormat);
    }
    val = val ? val : new Date(1970, 0, 1, 0, 0, 0);
    var da;
    if (val instanceof Date) {
      this.setDate(val);
      this.setTime(val);
      this.dateValue = new Date(Ext.isIE ? val.getTime() : val);
    } else {
      da = val.split(this.dtSeparator);
      this.setDate(da[0]);
      if (da[1]) {
        if (da[2]) {
          // add am/pm part back to time
          da[1] += da[2];
        }
        this.setTime(da[1]);
      }
    }
    this.updateValue();
    this.value = this.getValue();
  },

  /**
   * Hide or show this component by boolean
   *
   * @return {Ext.Component} this
   */
  setVisible : function(visible) {
    if (visible) {
      this.df.show();
      this.tf.show();
    } else {
      this.df.hide();
      this.tf.hide();
    }
    return this;
  },

  show : function() {
    return this.setVisible(true);
  },

  hide : function() {
    return this.setVisible(false);
  },

  /**
   * @private Updates the date part
   */
  updateDate : function() {
    var d = this.df.getValue();
    if (d) {
      if (!(this.dateValue instanceof Date)) {
        this.initDateValue();
        if (!this.tf.getValue()) {
          this.setTime(this.dateValue);
        }
      }
      this.dateValue.setMonth(0); // because of leap years
      this.dateValue.setFullYear(d.getFullYear());
      this.dateValue.setMonth(d.getMonth(), d.getDate());
      // this.dateValue.setDate(d.getDate());
    } else {
      this.dateValue = '';
      this.setTime('');
    }
  },


  /**
   * @private Updates the time part
   */
  updateTime : function() {
    var t = this.tf.getValue();
    if (t && !(t instanceof Date)) {
      t = Date.parseDate(t, this.tf.format);
    }
    if (t && !this.df.getValue()) {
      this.initDateValue();
      this.setDate(this.dateValue);
    }
    if (this.dateValue instanceof Date) {
      if (t) {
        this.dateValue.setHours(t.getHours());
        this.dateValue.setMinutes(t.getMinutes());
        this.dateValue.setSeconds(t.getSeconds());
      } else {
        this.dateValue.setHours(0);
        this.dateValue.setMinutes(0);
        this.dateValue.setSeconds(0);
      }
    }
  },


  /**
   * @private Updates the underlying hidden field value
   */
  updateHidden : function() {
    if (this.isRendered) {
      var value = this.dateValue instanceof Date ? this.dateValue.format(this.hiddenFormat) : '';
      this.el.dom.value = value;
    }
  },


  /**
   * @private Updates all of Date, Time and Hidden
   */
  updateValue : function() {

    this.updateDate();
    this.updateTime();
    this.updateHidden();

    return;
  },

  /**
   * @return {Boolean} true = valid, false = invalid calls validate methods of
   *         DateField and TimeField
   */
  validate : function() {
    return this.df.validate() && this.tf.validate();
  },


  /**
   * Returns renderer suitable to render this field
   *
   * @param {Object}
   *          Column model config
   */
  renderer : function(field) {
    var format = field.editor.dateFormat || Ext.ux.form.TwinDateTimeField.prototype.dateFormat;
    format += ' ' + (field.editor.timeFormat || Ext.ux.form.TwinDateTimeField.prototype.timeFormat);
    var renderer = function(val) {
      var retval = Ext.util.Format.date(val, format);
      return retval;
    };
    return renderer;
  }
});


/**
 *
 */

// create namespace
Ext.ns('Ext.ux');

/**
 *
 * @class Ext.ux.Window
 * @extends Ext.Window
 */
Ext.ux.Window = Ext.extend(Ext.Window, {

  initComponent : function() {
    Ext.Window.superclass.initComponent.call(this);
    Ext.EventManager.onWindowResize(this.keepItVisible, this, [true]);
    this.originalWidth = 0;
    this.originalHeight = 0;
    /* exclusive window */
    if (tvheadend.dialog)
      tvheadend.dialog.close();
    tvheadend.dialog = this;
  },

  beforeDestroy : function() {
    Ext.EventManager.removeResizeListener(this.keepItVisible, this);
    Ext.Window.superclass.beforeDestroy.call(this);
    tvheadend.dialog = null;
  },

  keepItVisible : function(resize) {
    var w = this.getWidth();
    var h = this.getHeight();
    var aw = Ext.lib.Dom.getViewWidth();
    var ah = Ext.lib.Dom.getViewHeight();
    var c = 0;

    if (resize && this.originalWidth) {
      w = this.originalWidth;
      c = 1;
    }
    if (resize && this.originalHeight) {
      h = this.originalHeight;
      c = 1;
    }

    if (w > aw) {
      w = aw;
      c = 1;
    }
    if (h > ah) {
      h = ah;
      c = 1;
    }
    if (c) {
      this.autoWidth = false;
      this.autoHeight = false;
      if (w === this.originalWidth)
        w = w + 15;
      this.setSize(w, h);
      this.center();
    } else if (resize) {
      this.center();
    } else {
      return false;
    }
    return true;
  },

  setOriginSize : function(force) {
    var w = this.getWidth();
    var h = this.getHeight();
    if (w > 200 && (force || this.originalWidth === 0))
      this.originalWidth = w;
    if (h > 100 && (force || this.originalHeight === 0))
      this.originalHeight = h;
    if (force && this.keepItVisible() === false)
      this.center();
  },

  onShow : function() {
    this.setOriginSize(false);
    this.keepItVisible();
  },

  onResize : function() {
    Ext.Window.superclass.onResize.apply(this, arguments);
    this.keepItVisible(false);
  },

});

// create namespace
Ext.ns('Ext.ux');

Ext.layout.Column2Layout = Ext.extend(Ext.layout.ColumnLayout, {

  getLayoutTargetSize : function() {
    var ret = Ext.layout.ColumnLayout.prototype.getLayoutTargetSize.call(this);
    if (ret && ret.width > 20)
      ret.width -= 20;
    return ret;
  }

});

Ext.Container.LAYOUTS['column2'] = Ext.layout.Column2Layout;
