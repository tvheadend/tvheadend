/*
 * Ext JS Library 2.1
 * Copyright(c) 2006-2008, Ext JS, LLC.
 * licensing@extjs.com
 * 
 * http://extjs.com/license
 */



/**
 * CheckedColumn
 */

Ext.grid.CheckColumn = function(config){
    Ext.apply(this, config);
    if(!this.id){
        this.id = Ext.id();
    }
    this.renderer = this.renderer.createDelegate(this);
};

Ext.grid.CheckColumn.prototype ={
    init : function(grid){
        this.grid = grid;
        this.grid.on('render', function(){
            var view = this.grid.getView();
            view.mainBody.on('mousedown', this.onMouseDown, this);
        }, this);
    },

    onMouseDown : function(e, t){
        if(t.className && t.className.indexOf('x-grid3-cc-'+this.id) != -1){
            e.stopEvent();
            var index = this.grid.getView().findRowIndex(t);
            var record = this.grid.store.getAt(index);
            record.set(this.dataIndex, !record.data[this.dataIndex]);
        }
    },

    renderer : function(v, p, record){
        p.css += ' x-grid3-check-col-td'; 
        return '<div class="x-grid3-check-col'+(v?'-on':'')+' x-grid3-cc-'+this.id+'">&#160;</div>';
    }
};


/**
 * ColumnTree
 */


Ext.tree.ColumnTree = Ext.extend(Ext.tree.TreePanel, {
    lines:false,
    borderWidth: Ext.isBorderBox ? 0 : 2, // the combined left/right border for each cell
    cls:'x-column-tree',
    
    onRender : function(){
        Ext.tree.ColumnTree.superclass.onRender.apply(this, arguments);
        this.headers = this.body.createChild(
            {cls:'x-tree-headers'},this.innerCt.dom);

        var cols = this.columns, c;
        var totalWidth = 0;

        for(var i = 0, len = cols.length; i < len; i++){
             c = cols[i];
             totalWidth += c.width;
             this.headers.createChild({
                 cls:'x-tree-hd ' + (c.cls?c.cls+'-hd':''),
                 cn: {
                     cls:'x-tree-hd-text',
                     html: c.header
                 },
                 style:'width:'+(c.width-this.borderWidth)+'px;'
             });
        }
        this.headers.createChild({cls:'x-clear'});
        // prevent floats from wrapping when clipped
        this.headers.setWidth(totalWidth);
        this.innerCt.setWidth(totalWidth);
    }
});

Ext.tree.ColumnNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {

    focus: Ext.emptyFn, // prevent odd scrolling behavior

    setColText : function(colidx, text) {
	    this.colNode[colidx].innerHTML = text;
	},

    renderElements : function(n, a, targetNode, bulkRender){
        this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';
	this.colNode = [];
	

        var t = n.getOwnerTree();
        var cols = t.columns;
        var bw = t.borderWidth;
        var c = cols[0];

        var buf = [
             '<li class="x-tree-node"><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                '<div class="x-tree-col" style="width:',c.width-bw,'px;">',
                    '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    '<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
                    '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    '<span unselectable="on">', n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</span></a>",
                "</div>"];
         for(var i = 1, len = cols.length; i < len; i++){
             c = cols[i];

             buf.push('<div class="x-tree-col ',(c.cls?c.cls:''),'" style="width:',c.width-bw,'px;">',
                        '<div class="x-tree-col-text">',(c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</div>",
                      "</div>");
         }
         buf.push(
            '<div class="x-clear"></div></div>',
            '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            "</li>");

        if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                n.nextSibling.ui.getEl(), buf.join(""));
        }else{
            this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        }

        this.elNode = this.wrap.childNodes[0];
        this.ctNode = this.wrap.childNodes[1];
        var cs = this.elNode.firstChild.childNodes;
        this.indentNode = cs[0];
        this.ecNode = cs[1];
        this.iconNode = cs[2];
        this.anchor = cs[3];
        this.textNode = cs[3].firstChild;

        for(var i = 1, len = cols.length; i < len; i++) {
	    this.colNode[i] = this.elNode.childNodes[i].firstChild;
	}
    }
});




Ext.grid.RowExpander = function(config){
    Ext.apply(this, config);

    this.addEvents({
        beforeexpand : true,
        expand: true,
        beforecollapse: true,
        collapse: true
    });

    Ext.grid.RowExpander.superclass.constructor.call(this);

    if(this.tpl){
        if(typeof this.tpl == 'string'){
            this.tpl = new Ext.Template(this.tpl);
        }
        this.tpl.compile();
    }

    this.state = {};
    this.bodyContent = {};
};

Ext.extend(Ext.grid.RowExpander, Ext.util.Observable, {
    header: "",
    width: 20,
    sortable: false,
    fixed:true,
    menuDisabled:true,
    dataIndex: '',
    id: 'expander',
    lazyRender : true,
    enableCaching: true,

    getRowClass : function(record, rowIndex, p, ds){
        p.cols = p.cols-1;
        var content = this.bodyContent[record.id];
        if(!content && !this.lazyRender){
            content = this.getBodyContent(record, rowIndex);
        }
        if(content){
            p.body = content;
        }
        return this.state[record.id] ? 'x-grid3-row-expanded' : 'x-grid3-row-collapsed';
    },

    init : function(grid){
        this.grid = grid;

        var view = grid.getView();
        view.getRowClass = this.getRowClass.createDelegate(this);

        view.enableRowBody = true;

        grid.on('render', function(){
            view.mainBody.on('mousedown', this.onMouseDown, this);
        }, this);
    },

    getBodyContent : function(record, index){
        if(!this.enableCaching){
            return this.tpl.apply(record.data);
        }
        var content = this.bodyContent[record.id];
        if(!content){
            content = this.tpl.apply(record.data);
            this.bodyContent[record.id] = content;
        }
        return content;
    },

    onMouseDown : function(e, t){
        if(t.className == 'x-grid3-row-expander'){
            e.stopEvent();
            var row = e.getTarget('.x-grid3-row');
            this.toggleRow(row);
        }
    },

    renderer : function(v, p, record){
        p.cellAttr = 'rowspan="2"';
        return '<div class="x-grid3-row-expander">&#160;</div>';
    },

    beforeExpand : function(record, body, rowIndex){
        if(this.fireEvent('beforeexpand', this, record, body, rowIndex) !== false){
            if(this.tpl && this.lazyRender){
                body.innerHTML = this.getBodyContent(record, rowIndex);
            }
            return true;
        }else{
            return false;
        }
    },

    toggleRow : function(row){
        if(typeof row == 'number'){
            row = this.grid.view.getRow(row);
        }
        this[Ext.fly(row).hasClass('x-grid3-row-collapsed') ? 'expandRow' : 'collapseRow'](row);
    },

    expandRow : function(row){
        if(typeof row == 'number'){
            row = this.grid.view.getRow(row);
        }
        var record = this.grid.store.getAt(row.rowIndex);
        var body = Ext.DomQuery.selectNode('tr:nth(2) div.x-grid3-row-body', row);
        if(this.beforeExpand(record, body, row.rowIndex)){
            this.state[record.id] = true;
            Ext.fly(row).replaceClass('x-grid3-row-collapsed', 'x-grid3-row-expanded');
            this.fireEvent('expand', this, record, body, row.rowIndex);
        }
    },

    collapseRow : function(row){
        if(typeof row == 'number'){
            row = this.grid.view.getRow(row);
        }
        var record = this.grid.store.getAt(row.rowIndex);
        var body = Ext.fly(row).child('tr:nth(1) div.x-grid3-row-body', true);
        if(this.fireEvent('beforecollapse', this, record, body, row.rowIndex) !== false){
            this.state[record.id] = false;
            Ext.fly(row).replaceClass('x-grid3-row-expanded', 'x-grid3-row-collapsed');
            this.fireEvent('collapse', this, record, body, row.rowIndex);
        }
    }
});



/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2008, Nige "Animal" White
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *     * Neither the name of the original author nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @class Ext.ux.DDView
 * <p>A DnD-enabled version of {@link Ext.DataView}. Drag/drop is implemented by adding
 * {@link Ext.data.Record}s to the target DDView. If copying is not being performed,
 * the original {@link Ext.data.Record} is removed from the source DDView.</p>
 * @constructor
 * Create a new DDView
 * @param {Object} config The configuration properties.
 */
Ext.ux.DDView = function(config) {
    if (!config.itemSelector) {
        var tpl = config.tpl;
        if (this.classRe.test(tpl)) {
            config.tpl = tpl.replace(this.classRe, 'class=$1x-combo-list-item $2$1');
        }
        else {
            config.tpl = tpl.replace(this.tagRe, '$1 class="x-combo-list-item" $2');
        }
        config.itemSelector = ".x-combo-list-item";
    }
    Ext.ux.DDView.superclass.constructor.call(this, Ext.apply(config, {
        border: false
    }));
};

Ext.extend(Ext.ux.DDView, Ext.DataView, {
    /**
     * @cfg {String/Array} dragGroup The ddgroup name(s) for the View's DragZone (defaults to undefined).
     */
    /**
     * @cfg {String/Array} dropGroup The ddgroup name(s) for the View's DropZone (defaults to undefined).
     */
    /**
     * @cfg {Boolean} copy Causes drag operations to copy nodes rather than move (defaults to false).
     */
    /**
     * @cfg {Boolean} allowCopy Causes ctrl/drag operations to copy nodes rather than move (defaults to false).
     */
    /**
     * @cfg {String} sortDir Sort direction for the view, 'ASC' or 'DESC' (defaults to 'ASC').
     */
    sortDir: 'ASC',

    // private
    isFormField: true,
    classRe: /class=(['"])(.*)\1/,
    tagRe: /(<\w*)(.*?>)/,
    reset: Ext.emptyFn,
    clearInvalid: Ext.form.Field.prototype.clearInvalid,

    // private
    afterRender: function() {
        Ext.ux.DDView.superclass.afterRender.call(this);
        if (this.dragGroup) {
            this.setDraggable(this.dragGroup.split(","));
        }
        if (this.dropGroup) {
            this.setDroppable(this.dropGroup.split(","));
        }
        if (this.deletable) {
            this.setDeletable();
        }
        this.isDirtyFlag = false;
        this.addEvents(
            "drop"
        );
    },

    // private
    validate: function() {
        return true;
    },

    // private
    destroy: function() {
        this.purgeListeners();
        this.getEl().removeAllListeners();
        this.getEl().remove();
        if (this.dragZone) {
            if (this.dragZone.destroy) {
                this.dragZone.destroy();
            }
        }
        if (this.dropZone) {
            if (this.dropZone.destroy) {
                this.dropZone.destroy();
            }
        }
    },

	/**
	 * Allows this class to be an Ext.form.Field so it can be found using {@link Ext.form.BasicForm#findField}.
	 */
    getName: function() {
        return this.name;
    },

	/**
	 * Loads the View from a JSON string representing the Records to put into the Store.
     * @param {String} value The JSON string
	 */
    setValue: function(v) {
        if (!this.store) {
            throw "DDView.setValue(). DDView must be constructed with a valid Store";
        }
        var data = {};
        data[this.store.reader.meta.root] = v ? [].concat(v) : [];
        this.store.proxy = new Ext.data.MemoryProxy(data);
        this.store.load();
    },

	/**
	 * Returns the view's data value as a list of ids.
     * @return {String} A parenthesised list of the ids of the Records in the View, e.g. (1,3,8).
	 */
    getValue: function() {
        var result = '(';
        this.store.each(function(rec) {
            result += rec.id + ',';
        });
        return result.substr(0, result.length - 1) + ')';
    },

    getIds: function() {
        var i = 0, result = new Array(this.store.getCount());
        this.store.each(function(rec) {
            result[i++] = rec.id;
        });
        return result;
    },

    /**
     * Returns true if the view's data has changed, else false.
     * @return {Boolean}
     */
    isDirty: function() {
        return this.isDirtyFlag;
    },

	/**
	 * Part of the Ext.dd.DropZone interface. If no target node is found, the
	 * whole Element becomes the target, and this causes the drop gesture to append.
	 */
    getTargetFromEvent : function(e) {
        var target = e.getTarget();
        while ((target !== null) && (target.parentNode != this.el.dom)) {
            target = target.parentNode;
        }
        if (!target) {
            target = this.el.dom.lastChild || this.el.dom;
        }
        return target;
    },

	/**
	 * Create the drag data which consists of an object which has the property "ddel" as
	 * the drag proxy element.
	 */
    getDragData : function(e) {
        var target = this.findItemFromChild(e.getTarget());
        if(target) {
            if (!this.isSelected(target)) {
                delete this.ignoreNextClick;
                this.onItemClick(target, this.indexOf(target), e);
                this.ignoreNextClick = true;
            }
            var dragData = {
                sourceView: this,
                viewNodes: [],
                records: [],
                copy: this.copy || (this.allowCopy && e.ctrlKey)
            };
            if (this.getSelectionCount() == 1) {
                var i = this.getSelectedIndexes()[0];
                var n = this.getNode(i);
                dragData.viewNodes.push(dragData.ddel = n);
                dragData.records.push(this.store.getAt(i));
                dragData.repairXY = Ext.fly(n).getXY();
            } else {
                dragData.ddel = document.createElement('div');
                dragData.ddel.className = 'multi-proxy';
                this.collectSelection(dragData);
            }
            return dragData;
        }
        return false;
    },

    // override the default repairXY.
    getRepairXY : function(e){
        return this.dragData.repairXY;
    },

	// private
    collectSelection: function(data) {
        data.repairXY = Ext.fly(this.getSelectedNodes()[0]).getXY();
        if (this.preserveSelectionOrder === true) {
            Ext.each(this.getSelectedIndexes(), function(i) {
                var n = this.getNode(i);
                var dragNode = n.cloneNode(true);
                dragNode.id = Ext.id();
                data.ddel.appendChild(dragNode);
                data.records.push(this.store.getAt(i));
                data.viewNodes.push(n);
            }, this);
        } else {
            var i = 0;
            this.store.each(function(rec){
                if (this.isSelected(i)) {
                    var n = this.getNode(i);
                    var dragNode = n.cloneNode(true);
                    dragNode.id = Ext.id();
                    data.ddel.appendChild(dragNode);
                    data.records.push(this.store.getAt(i));
                    data.viewNodes.push(n);
                }
                i++;
            }, this);
        }
    },

	/**
	 * Specify to which ddGroup items in this DDView may be dragged.
     * @param {String} ddGroup The DD group name to assign this view to.
	 */
    setDraggable: function(ddGroup) {
        if (ddGroup instanceof Array) {
            Ext.each(ddGroup, this.setDraggable, this);
            return;
        }
        if (this.dragZone) {
            this.dragZone.addToGroup(ddGroup);
        } else {
            this.dragZone = new Ext.dd.DragZone(this.getEl(), {
                containerScroll: true,
                ddGroup: ddGroup
            });
            // Draggability implies selection. DragZone's mousedown selects the element.
            if (!this.multiSelect) { this.singleSelect = true; }

            // Wire the DragZone's handlers up to methods in *this*
            this.dragZone.getDragData = this.getDragData.createDelegate(this);
            this.dragZone.getRepairXY = this.getRepairXY;
            this.dragZone.onEndDrag = this.onEndDrag;
        }
    },

	/**
	 * Specify from which ddGroup this DDView accepts drops.
     * @param {String} ddGroup The DD group name from which to accept drops.
	 */
    setDroppable: function(ddGroup) {
        if (ddGroup instanceof Array) {
            Ext.each(ddGroup, this.setDroppable, this);
            return;
        }
        if (this.dropZone) {
            this.dropZone.addToGroup(ddGroup);
        } else {
            this.dropZone = new Ext.dd.DropZone(this.getEl(), {
                owningView: this,
                containerScroll: true,
                ddGroup: ddGroup
            });

            // Wire the DropZone's handlers up to methods in *this*
            this.dropZone.getTargetFromEvent = this.getTargetFromEvent.createDelegate(this);
            this.dropZone.onNodeEnter = this.onNodeEnter.createDelegate(this);
            this.dropZone.onNodeOver = this.onNodeOver.createDelegate(this);
            this.dropZone.onNodeOut = this.onNodeOut.createDelegate(this);
            this.dropZone.onNodeDrop = this.onNodeDrop.createDelegate(this);
        }
    },

	// private
    getDropPoint : function(e, n, dd){
        if (n == this.el.dom) { return "above"; }
        var t = Ext.lib.Dom.getY(n), b = t + n.offsetHeight;
        var c = t + (b - t) / 2;
        var y = Ext.lib.Event.getPageY(e);
        if(y <= c) {
            return "above";
        }else{
            return "below";
        }
    },

    // private
    isValidDropPoint: function(pt, n, data) {
        if (!data.viewNodes || (data.viewNodes.length != 1)) {
            return true;
        }
        var d = data.viewNodes[0];
        if (d == n) {
            return false;
        }
        if ((pt == "below") && (n.nextSibling == d)) {
            return false;
        }
        if ((pt == "above") && (n.previousSibling == d)) {
            return false;
        }
        return true;
    },

    // private
    onNodeEnter : function(n, dd, e, data){
        if (this.highlightColor && (data.sourceView != this)) {
            this.el.highlight(this.highlightColor);
        }
        return false;
    },

    // private
    onNodeOver : function(n, dd, e, data){
        var dragElClass = this.dropNotAllowed;
        var pt = this.getDropPoint(e, n, dd);
        if (this.isValidDropPoint(pt, n, data)) {
            if (this.appendOnly || this.sortField) {
                return "x-tree-drop-ok-below";
            }

            // set the insert point style on the target node
            if (pt) {
                var targetElClass;
                if (pt == "above"){
                    dragElClass = n.previousSibling ? "x-tree-drop-ok-between" : "x-tree-drop-ok-above";
                    targetElClass = "x-view-drag-insert-above";
                } else {
                    dragElClass = n.nextSibling ? "x-tree-drop-ok-between" : "x-tree-drop-ok-below";
                    targetElClass = "x-view-drag-insert-below";
                }
                if (this.lastInsertClass != targetElClass){
                    Ext.fly(n).replaceClass(this.lastInsertClass, targetElClass);
                    this.lastInsertClass = targetElClass;
                }
            }
        }
        return dragElClass;
    },

    // private
    onNodeOut : function(n, dd, e, data){
        this.removeDropIndicators(n);
    },

    // private
    onNodeDrop : function(n, dd, e, data){
        if (this.fireEvent("drop", this, n, dd, e, data) === false) {
            return false;
        }
        var pt = this.getDropPoint(e, n, dd);
        var insertAt = (this.appendOnly || (n == this.el.dom)) ? this.store.getCount() : n.viewIndex;
        if (pt == "below") {
            insertAt++;
        }

        // Validate if dragging within a DDView
        if (data.sourceView == this) {
            // If the first element to be inserted below is the target node, remove it
            if (pt == "below") {
                if (data.viewNodes[0] == n) {
                    data.viewNodes.shift();
                }
            } else {  // If the last element to be inserted above is the target node, remove it
                if (data.viewNodes[data.viewNodes.length - 1] == n) {
                    data.viewNodes.pop();
                }
            }

            // Nothing to drop...
            if (!data.viewNodes.length) {
                return false;
            }

            // If we are moving DOWN, then because a store.remove() takes place first,
            // the insertAt must be decremented.
            if (insertAt > this.store.indexOf(data.records[0])) {
                insertAt--;
            }
        }

        // Dragging from a Tree. Use the Tree's recordFromNode function.
        if (data.node instanceof Ext.tree.TreeNode) {
            var r = data.node.getOwnerTree().recordFromNode(data.node);
            if (r) {
                data.records = [ r ];
            }
        }

        if (!data.records) {
            alert("Programming problem. Drag data contained no Records");
            return false;
        }

        for (var i = 0; i < data.records.length; i++) {
            var r = data.records[i];
            var dup = this.store.getById(r.id);
            if (dup && (dd != this.dragZone)) {
                if(!this.allowDup && !this.allowTrash){
                    Ext.fly(this.getNode(this.store.indexOf(dup))).frame("red", 1);
                    return true
                }
                var x=new Ext.data.Record();
                r.id=x.id;
                delete x;
            }
            if (data.copy) {
                this.store.insert(insertAt++, r.copy());
            } else {
                if (data.sourceView) {
                    data.sourceView.isDirtyFlag = true;
                    data.sourceView.store.remove(r);
                }
                if(!this.allowTrash)this.store.insert(insertAt++, r);
            }
            if(this.sortField){
                this.store.sort(this.sortField, this.sortDir);
            }
            this.isDirtyFlag = true;
        }
        this.dragZone.cachedTarget = null;
        return true;
    },

    // private
    onEndDrag: function(data, e) {
        var d = Ext.get(this.dragData.ddel);
        if (d && d.hasClass("multi-proxy")) {
            d.remove();
            //delete this.dragData.ddel;
        }
    },

    // private
    removeDropIndicators : function(n){
        if(n){
            Ext.fly(n).removeClass([
                "x-view-drag-insert-above",
                "x-view-drag-insert-left",
                "x-view-drag-insert-right",
                "x-view-drag-insert-below"]);
            this.lastInsertClass = "_noclass";
        }
    },

	/**
	 * Add a delete option to the DDView's context menu.
	 * @param {String} imageUrl The URL of the "delete" icon image.
	 */
    setDeletable: function(imageUrl) {
        if (!this.singleSelect && !this.multiSelect) {
            this.singleSelect = true;
        }
        var c = this.getContextMenu();
        this.contextMenu.on("itemclick", function(item) {
            switch (item.id) {
                case "delete":
                    this.remove(this.getSelectedIndexes());
                    break;
            }
        }, this);
        this.contextMenu.add({
            icon: imageUrl || AU.resolveUrl("/images/delete.gif"),
            id: "delete",
            text: AU.getMessage("deleteItem")
        });
    },

	/**
	 * Return the context menu for this DDView.
     * @return {Ext.menu.Menu} The context menu
	 */
    getContextMenu: function() {
        if (!this.contextMenu) {
            // Create the View's context menu
            this.contextMenu = new Ext.menu.Menu({
                id: this.id + "-contextmenu"
            });
            this.el.on("contextmenu", this.showContextMenu, this);
        }
        return this.contextMenu;
    },

    /**
     * Disables the view's context menu.
     */
    disableContextMenu: function() {
        if (this.contextMenu) {
            this.el.un("contextmenu", this.showContextMenu, this);
        }
    },

    // private
    showContextMenu: function(e, item) {
        item = this.findItemFromChild(e.getTarget());
        if (item) {
            e.stopEvent();
            this.select(this.getNode(item), this.multiSelect && e.ctrlKey, true);
            this.contextMenu.showAt(e.getXY());
        }
    },

	/**
	 * Remove {@link Ext.data.Record}s at the specified indices.
	 * @param {Array/Number} selectedIndices The index (or Array of indices) of Records to remove.
	 */
    remove: function(selectedIndices) {
        selectedIndices = [].concat(selectedIndices);
        for (var i = 0; i < selectedIndices.length; i++) {
            var rec = this.store.getAt(selectedIndices[i]);
            this.store.remove(rec);
        }
    },

	/**
	 * Double click fires the {@link #dblclick} event. Additionally, if this DDView is draggable, and there is only one other
	 * related DropZone that is in another DDView, it drops the selected node on that DDView.
	 */
    onDblClick : function(e){
        var item = this.findItemFromChild(e.getTarget());
        if(item){
            if (this.fireEvent("dblclick", this, this.indexOf(item), item, e) === false) {
                return false;
            }
            if (this.dragGroup) {
                var targets = Ext.dd.DragDropMgr.getRelated(this.dragZone, true);

                // Remove instances of this View's DropZone
                while (targets.indexOf(this.dropZone) !== -1) {
                    targets.remove(this.dropZone);
                }

                // If there's only one other DropZone, and it is owned by a DDView, then drop it in
                if ((targets.length == 1) && (targets[0].owningView)) {
                    this.dragZone.cachedTarget = null;
                    var el = Ext.get(targets[0].getEl());
                    var box = el.getBox(true);
                    targets[0].onNodeDrop(el.dom, {
                        target: el.dom,
                        xy: [box.x, box.y + box.height - 1]
                    }, null, this.getDragData(e));
                }
            }
        }
    },

    // private
    onItemClick : function(item, index, e){
        // The DragZone's mousedown->getDragData already handled selection
        if (this.ignoreNextClick) {
            delete this.ignoreNextClick;
            return;
        }

        if(this.fireEvent("beforeclick", this, index, item, e) === false){
            return false;
        }
        if(this.multiSelect || this.singleSelect){
            if(this.multiSelect && e.shiftKey && this.lastSelection){
                this.select(this.getNodes(this.indexOf(this.lastSelection), index), false);
            } else if (this.isSelected(item) && e.ctrlKey) {
                this.deselect(item);
            }else{
                this.deselect(item);
                this.select(item, this.multiSelect && e.ctrlKey);
                this.lastSelection = item;
            }
            e.preventDefault();
        }
        return true;
    }
});



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
Ext.ux.Multiselect = Ext.extend(Ext.form.Field,  {
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
    appendOnly:false,
    /**
     * @cfg {Array} dataFields Inline data definition when not using a pre-initialised store. Known to cause problems 
     * in some browswers for very long lists. Use store for large datasets.
     */
    dataFields:[],
    /**
     * @cfg {Array} data Inline data when not using a pre-initialised store. Known to cause problems in some 
     * browswers for very long lists. Use store for large datasets.
     */
    data:[],
    /**
     * @cfg {Number} width Width in pixels of the control (defaults to 100).
     */
    width:100,
    /**
     * @cfg {Number} height Height in pixels of the control (defaults to 100).
     */
    height:100,
    /**
     * @cfg {String/Number} displayField Name/Index of the desired display field in the dataset (defaults to 0).
     */
    displayField:0,
    /**
     * @cfg {String/Number} valueField Name/Index of the desired value field in the dataset (defaults to 1).
     */
    valueField:1,
    /**
     * @cfg {Boolean} allowBlank True to require at least one item in the list to be selected, false to allow no 
     * selection (defaults to true).
     */
    allowBlank:true,
    /**
     * @cfg {Number} minLength Minimum number of selections allowed (defaults to 0).
     */
    minLength:0,
    /**
     * @cfg {Number} maxLength Maximum number of selections allowed (defaults to Number.MAX_VALUE). 
     */
    maxLength:Number.MAX_VALUE,
    /**
     * @cfg {String} blankText Default text displayed when the control contains no items (defaults to the same value as
     * {@link Ext.form.TextField#blankText}.
     */
    blankText:Ext.form.TextField.prototype.blankText,
    /**
     * @cfg {String} minLengthText Validation message displayed when {@link #minLength} is not met (defaults to 'Minimum {0} 
     * item(s) required').  The {0} token will be replaced by the value of {@link #minLength}.
     */
    minLengthText:'Minimum {0} item(s) required',
    /**
     * @cfg {String} maxLengthText Validation message displayed when {@link #maxLength} is not met (defaults to 'Maximum {0} 
     * item(s) allowed').  The {0} token will be replaced by the value of {@link #maxLength}.
     */
    maxLengthText:'Maximum {0} item(s) allowed',
    /**
     * @cfg {String} delimiter The string used to delimit between items when set or returned as a string of values
     * (defaults to ',').
     */
    delimiter:',',
    
    // DDView settings
    copy:false,
    allowDup:false,
    allowTrash:false,
    focusClass:undefined,
    sortDir:'ASC',
    
    // private
    defaultAutoCreate : {tag: "div"},
    
    // private
    initComponent: function(){
        Ext.ux.Multiselect.superclass.initComponent.call(this);
        this.addEvents({
            'dblclick' : true,
            'click' : true,
            'change' : true,
            'drop' : true
        });     
    },
    
    // private
    onRender: function(ct, position){
        Ext.ux.Multiselect.superclass.onRender.call(this, ct, position);
        
        var cls = 'ux-mselect';
        var fs = new Ext.form.FieldSet({
            renderTo:this.el,
            title:this.legend,
            height:this.height,
            width:this.width,
            style:"padding:0;",
            tbar:this.tbar
        });
        //if(!this.legend)fs.el.down('.'+fs.headerCls).remove();
        fs.body.addClass(cls);

        var tpl = '<tpl for="."><div class="' + cls + '-item';
        if(Ext.isIE || Ext.isIE7){
            tpl+='" unselectable=on';
        }else{
            tpl+=' x-unselectable"';
        }
        tpl+='>{' + this.displayField + '}</div></tpl>';

        if(!this.store){
            this.store = new Ext.data.SimpleStore({
                fields: this.dataFields,
                data : this.data
            });
        }

        this.view = new Ext.ux.DDView({
            multiSelect: true, 
            store: this.store, 
            selectedClass: cls+"-selected", 
            tpl:tpl,
            allowDup:this.allowDup, 
            copy: this.copy, 
            allowTrash: this.allowTrash, 
            dragGroup: this.dragGroup, 
            dropGroup: this.dropGroup, 
            itemSelector:"."+cls+"-item",
            isFormField:false, 
            applyTo:fs.body,
            appendOnly:this.appendOnly,
            sortField:this.sortField, 
            sortDir:this.sortDir
        });

        fs.add(this.view);
        
        this.view.on('click', this.onViewClick, this);
        this.view.on('beforeClick', this.onViewBeforeClick, this);
        this.view.on('dblclick', this.onViewDblClick, this);
        this.view.on('drop', function(ddView, n, dd, e, data){
            return this.fireEvent("drop", ddView, n, dd, e, data);
        }, this);
        
        this.hiddenName = this.name;
        var hiddenTag={tag: "input", type: "hidden", value: "", name:this.name};
        if (this.isFormField) { 
            this.hiddenField = this.el.createChild(hiddenTag);
        } else {
            this.hiddenField = Ext.get(document.body).createChild(hiddenTag);
        }
        fs.doLayout();
    },
    
    // private
    initValue:Ext.emptyFn,
    
    // private
    onViewClick: function(vw, index, node, e) {
        var arrayIndex = this.preClickSelections.indexOf(index);
        if (arrayIndex  != -1)
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
        if (this.disabled) {return false;}
    },

    // private
    onViewDblClick : function(vw, index, node, e) {
        return this.fireEvent('dblclick', vw, index, node, e);
    },  
    
    /**
     * Returns an array of data values for the selected items in the list. The values will be separated
     * by {@link #delimiter}.
     * @return {Array} value An array of string data values
     */
    getValue: function(valueField){
        var returnArray = [];
        var selectionsArray = this.view.getSelectedIndexes();
        if (selectionsArray.length == 0) {return '';}
        for (var i=0; i<selectionsArray.length; i++) {
            returnArray.push(this.store.getAt(selectionsArray[i]).get(((valueField != null)? valueField : this.valueField)));
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
        
        if (!values || (values == '')) { return; }
        
        if (!(values instanceof Array)) { values = values.split(this.delimiter); }
        for (var i=0; i<values.length; i++) {
            index = this.view.store.indexOf(this.view.store.query(this.valueField, 
                new RegExp('^' + values[i] + '$', "i")).itemAt(0));
            selections.push(index);
        }
        this.view.select(selections);
        this.hiddenField.dom.value = this.getValue();
        this.validate();
    },
    
    // inherit docs
    reset : function() {
        this.setValue('');
    },
    
    // inherit docs
    getRawValue: function(valueField) {
        var tmp = this.getValue(valueField);
        if (tmp.length) {
            tmp = tmp.split(this.delimiter);
        }
        else{
            tmp = [];
        }
        return tmp;
    },

    // inherit docs
    setRawValue: function(values){
        setValue(values);
    },

    // inherit docs
    validateValue : function(value){
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
