// vim: ts=4:sw=4:nu:fdc=2:nospell
/**
 * Ext.ux.form.XCheckbox - nice checkbox with configurable submit values
 *
 * @author  Ing. Jozef Sakalos
 * @version $Id: Ext.ux.form.XCheckbox.js 81 2008-03-20 11:13:36Z jozo $
 * @date    10. February 2008
 *
 *
 * @license Ext.ux.form.XCheckbox is licensed under the terms of
 * the Open Source LGPL 3.0 license.  Commercial use is permitted to the extent
 * that the code/component(s) do NOT become part of another Open Source or Commercially
 * licensed development library or toolkit without explicit permission.
 * 
 * License details: http://www.gnu.org/licenses/lgpl.html
 */

/**
  * Default css:
  * .xcheckbox-wrap {
  *     line-height: 18px;
  *     padding-top:2px;
  * }
  * .xcheckbox-wrap a {
  *     display:block;
  *     width:16px;
  *     height:16px;
  *     float:left;
  * }
  * .x-toolbar .xcheckbox-wrap {
  *     padding: 0 0 2px 0;
  * }
  * .xcheckbox-on {
  *     background:transparent url(./ext/resources/images/default/menu/checked.gif) no-repeat 0 0;
  * }
  * .xcheckbox-off {
  *     background:transparent url(./ext/resources/images/default/menu/unchecked.gif) no-repeat 0 0;
  * }
  * .xcheckbox-disabled {
  *     opacity: 0.5;
  *     -moz-opacity: 0.5;
  *     filter: alpha(opacity=50);
  *     cursor:default;
  * }
  *
  * @class Ext.ux.XCheckbox
  * @extends Ext.form.Checkbox
  */
Ext.ns('Ext.ux.form');
Ext.ux.form.XCheckbox = Ext.extend(Ext.form.Checkbox, {
     offCls:'xcheckbox-off'
    ,onCls:'xcheckbox-on'
    ,disabledClass:'xcheckbox-disabled'
    ,submitOffValue:'false'
    ,submitOnValue:'true'
    ,checked:false

    ,onRender:function(ct) {
        // call parent
        Ext.ux.form.XCheckbox.superclass.onRender.apply(this, arguments);

        // save tabIndex remove & re-create this.el
        var tabIndex = this.el.dom.tabIndex;
        var id = this.el.dom.id;
        this.el.remove();
        this.el = ct.createChild({tag:'input', type:'hidden', name:this.name, id:id});

        // update value of hidden field
        this.updateHidden();

        // adjust wrap class and create link with bg image to click on
        this.wrap.replaceClass('x-form-check-wrap', 'xcheckbox-wrap');
        this.cbEl = this.wrap.createChild({tag:'a', href:'#', cls:this.checked ? this.onCls : this.offCls});

        // reposition boxLabel if any
        var boxLabel = this.wrap.down('label');
        if(boxLabel) {
            this.wrap.appendChild(boxLabel);
        }

        // support tooltip
        if(this.tooltip) {
            this.cbEl.set({qtip:this.tooltip});
        }

        // install event handlers
        this.wrap.on({click:{scope:this, fn:this.onClick, delegate:'a'}});
        this.wrap.on({keyup:{scope:this, fn:this.onClick, delegate:'a'}});

        // restore tabIndex
        this.cbEl.dom.tabIndex = tabIndex;
    } // eo function onRender

    ,onClick:function(e) {
        if(this.disabled || this.readOnly) {
            return;
        }
        if(!e.isNavKeyPress()) {
            this.setValue(!this.checked);
        }
    } // eo function onClick

    ,onDisable:function() {
        this.cbEl.addClass(this.disabledClass);
        this.el.dom.disabled = true;
    } // eo function onDisable

    ,onEnable:function() {
        this.cbEl.removeClass(this.disabledClass);
        this.el.dom.disabled = false;
    } // eo function onEnable

    ,setValue:function(val) {
        if('string' == typeof val) {
            this.checked = val === this.submitOnValue;
        }
        else {
            this.checked = !(!val);
        }

        if(this.rendered && this.cbEl) {
            this.updateHidden();
            this.cbEl.removeClass([this.offCls, this.onCls]);
            this.cbEl.addClass(this.checked ? this.onCls : this.offCls);
        }
        this.fireEvent('check', this, this.checked);

    } // eo function setValue

    ,updateHidden:function() {
        this.el.dom.value = this.checked ? this.submitOnValue : this.submitOffValue;
    } // eo function updateHidden

    ,getValue:function() {
        return this.checked;
    } // eo function getValue

}); // eo extend

// register xtype
Ext.reg('xcheckbox', Ext.ux.form.XCheckbox);
