/*
 * Combobox stores
 */
tvheadend.idnode_enum_stores = {}

tvheadend.idnode_get_enum = function(conf)
{
    /* Build key */
    var key = conf.url;
    if (conf.params)
        key += '?' + Ext.util.JSON.encode(conf.params);
    if (conf.event)
        key += '+' + conf.event;

    /* Use cached */
    if (key in tvheadend.idnode_enum_stores)
        return tvheadend.idnode_enum_stores[key];

    var stype = Ext.data.SortTypes.none;
    var sinfo = conf.sort;
    if (conf.stype !== 'none') {
        stype = Ext.data.SortTypes.asUCString;
        sinfo = { field: 'val', 'direction': 'ASC' };
    }

    /* Build store */
    var st = new Ext.data.JsonStore({
        root: conf.root || 'entries',
        url: conf.url,
        baseParams: conf.params || {},
        fields: conf.fields ||
            [
                'key',
                'val',
                {
                    name: 'val',
                    sortType: stype
                }
            ],
        id: conf.id || 'key',
        autoLoad: true,
        listeners: conf.listeners || {},
        sortInfo: sinfo
    });
    tvheadend.idnode_enum_stores[key] = st;

    /* Event to update */
    if (conf.event) {
        tvheadend.comet.on(conf.event, function() {
            st.reload();
        });
    }

    return st;
};

json_decode = function(d)
{
    if (d && d.responseText) {
        d = Ext.util.JSON.decode(d.responseText);
        if (d.entries)
            d = d.entries;
        if (d.nodes)
            d = d.nodes;
    } else {
        d = [];
    }
    return d;
};

/*
 * Build enum data store
 */
tvheadend.idnode_enum_store = function(f)
{
    var store = null;

    /* API fetch */
    if (f['enum'].type === 'api') {
        return tvheadend.idnode_get_enum({
            url: 'api/' + f['enum'].uri,
            params: f['enum'].params,
            event: f['enum'].event,
            stype: f['enum'].stype
        });
    }

    switch (f.type) {
        case 'str':
            if (f['enum'].length > 0 && f['enum'][0] instanceof Object)
                store = new Ext.data.JsonStore({
                    id: 'key',
                    fields: ['key', 'val'],
                    data: f['enum']
                });
            else
                store = f['enum'];
            break;
        case 'int':
        case 'u32':
        case 'u16':
        case 's64':
        case 'dbl':
        case 'time':
            var data = null;
            if (f['enum'].length > 0 && f['enum'][0] instanceof Object) {
                data = f['enum'];
            } else {
                data = [];
                for (i = 0; i < f['enum'].length; i++)
                    data.push({key: i, val: f['enum'][i]});
            }
            store = new Ext.data.JsonStore({
                id: 'key',
                fields: ['key', 'val'],
                data: data
            });
            break;
    }
    return store;
};

tvheadend.idnode_enum_select_store = function(conf, st, to)
{
  var r = new Ext.data.JsonStore({
      id: 'key',
      fields: ['key', 'val'],
      data: []
  });

  function changed_from(st) {
      r.loadData([], false); /* remove all data */
      if (conf.value) {
          st.each(function(rec) {
              if (conf.value.indexOf(rec.id) < 0) {
                  var nrec = new r.recordType({key: rec.data.key, val: rec.data.val});
                  nrec.id = rec.id;
                  r.add(nrec);
              }
          });
      }
  }

  function changed_to(st) {
      r.loadData([], false); /* remove all data */
      for (var i = 0; i < conf.value.length; i++) {
          var rec = st.getById(conf.value[i]);
          if (rec) {
              var nrec = new r.recordType({key: rec.data.key, val: rec.data.val});
              nrec.id = rec.id;
              r.add(nrec);
          }
      }
  }

  if (to) {
    st.on('load', changed_to);
    changed_to(st);
  } else {
    st.on('load', changed_from);
    changed_from(st);
  }

  return r;
}

tvheadend.idnode_filter_fields = function(d, list)
{
    if (!list)
        return d;
    var r = [];
    if (list.charAt(0) === '-') {
        var o = list.substring(1).split(',');
        r = d.slice();
        for (var i = 0; i < o.length; i++)
           for (var j = 0; j < r.length; j++)
              if (r[j].id === o[i]) {
                  r.splice(i, 1);
                  break;
              }
    } else {
        var o = list.split(',');
        for (var i = 0; i < o.length; i++)
           for (var j = 0; j < d.length; j++)
              if (d[j].id === o[i]) {
                  r.push(d[j]);
                  break;
              }
    }
    return r;
}

Ext.ux.grid.filter.IntsplitFilter = Ext.extend(Ext.ux.grid.filter.NumericFilter, {

    fieldCls : Ext.form.TextField,

    constructor: function(conf) {
        this.intsplit = conf.intsplit;
        if (!conf.fields)
            conf.fields = {
                gt: { maskRe: /[0-9\.]/ },
                lt: { maskRe: /[0-9\.]/ },
                eq: { maskRe: /[0-9\.]/ }
            };
        Ext.ux.grid.filter.IntsplitFilter.superclass.constructor.call(this, conf);
    },

    getSerialArgs: function () {
        var key,
        args = [],
        values = this.menu.getValue();
        for (key in values) {
            var s = values[key].toString().split('.');
            var v = 0;
            if (s.length > 0)
                v = parseInt(s[0]) * this.intsplit;
            if (s.length > 1)
                v += parseInt(s[1]);
            args.push({
                type: 'numeric',
                comparison: key,
                value: v,
                intsplit: this.intsplit
            });
        }
        return args;
    }

});

tvheadend.IdNodeField = function(conf)
{
    /*
     * Properties
     */
    this.id = conf.id;
    this.text = conf.caption || this.id;
    this.description = conf.description || null;
    this.type = conf.type;
    this.list = conf.list;
    this.rdonly = conf.rdonly;
    this.wronly = conf.wronly;
    this.wronce = conf.wronce;
    this.noui = conf.noui;
    this.hidden = conf.hidden;
    this.uilevel = conf.expert ? 'expert' : (conf.advanced ? 'advanced' : 'basic');
    this.password = conf.showpwd ? false : conf.password;
    this.duration = conf.duration;
    this.date = conf.date;
    this.intsplit = conf.intsplit;
    this.hexa = conf.hexa;
    this.group = conf.group;
    this.lorder = conf.lorder;
    this.multiline = conf.multiline;
    this['enum'] = conf['enum'];
    this.store = null;
    if (this['enum'])
        this.store = tvheadend.idnode_enum_store(this);
    if (this.lorder) {
        this.fromstore = tvheadend.idnode_enum_select_store(this, this.store, false);
        this.tostore = tvheadend.idnode_enum_select_store(this, this.store, true);
    }
    this.ordered = false;

    /*
     * Methods
     */

    this.onrefresh = function(callback) {
        var st = this.store;
        if (st && st instanceof Ext.data.JsonStore)
            st.on('load', callback);
    }

    this.unrefresh = function(callback) {
        var st = this.store;
        if (st && st instanceof Ext.data.JsonStore)
            st.un('load', callback);
    }

    this.get_hidden = function(uilevel) {
        var hidden = this.hidden || this.noui;
        if (!tvheadend.uilevel_match(this.uilevel, uilevel))
            hidden = true;
        return hidden;
    }

    this.column = function(uilevel, conf)
    {
        var cfg = conf && this.id in conf ? conf[this.id] : {};
        var w = 300;
        var ftype = 'string';
        if (this.intsplit) {
            ftype = 'intsplit';
            w = 80;
        } else if (this.type === 'int' || this.type === 'u32' ||
            this.type === 'u16' || this.type === 's64' ||
            this.type === 'dbl') {
            ftype = this.hexa ? 'string' : 'numeric';
            w = 80;
        } else if (this.type === 'time') {
            w = 140;
            ftype = 'date';
            if (this.duration) {
              ftype = 'numeric';
              w = 80;
            }
        } else if (this.type === 'bool') {
            ftype = 'boolean';
            w = 60;
        }
        if (this['enum'] || this.list)
            w = 300;

        var props = {
            width: cfg.width || w,
            dataIndex: this.id,
            header: this.text,
            editor: this.editor({create: false}),
            renderer: cfg.renderer ? cfg.renderer(this.store) : this.renderer(this.store),
            editable: !this.rdonly,
            hidden: this.get_hidden(uilevel),
            filter: {
                type: ftype,
                dataIndex: this.id,
                intsplit: this.intsplit
            }
        };

        // Special handling for checkboxes
        if (ftype === 'boolean')
        {
            props.xtype = 'checkcolumn';
            props.renderer = Ext.ux.grid.CheckColumn.prototype.renderer;
        }
        
        return props;
    };

    this.renderer = function(st)
    {
        if (this.password)
            return function(v) {
                return '<span class="tvh-grid-unset">********</span>';
            }
            
        if (this.type === 'time') {
            if (this.duration)
                return function(v) {
                    if (v < 0 || v === '')
                       return _("Not set");
                    var i = parseInt(v / 60); /* Nevermind the seconds */
                    if (i === 0)
                       return "0";
                    var hours = parseInt(i / 60);
                    var min = parseInt(i % 60);
                    if (hours) {
                        if (min === 0)
                            return hours + ' ' + _('hrs');
                        return hours + ' ' + _('hrs') + ', ' + min + ' ' + _('min');
                    }
                    return min + ' ' + _('min');
                }
            if (this.date) {
                return function(v) {
                    if (v > 0) {
                        var dt = new Date(v * 1000);
                        return dt.toLocaleDateString();
                    }
                    return '';
                }
            }
            return function(v) {
                if (v > 0) {
                    var dt = new Date(v * 1000);
                    var wd = dt.toLocaleString(window.navigator.language, {weekday: 'short'});
                    return wd + ' ' + dt.toLocaleString();
                }
                return '';
            }
        }

        if (!st)
            return null;

        return function(v) {
            if (st && st instanceof Ext.data.JsonStore) {
                var t = [];
                var d = v.push ? v : [v];
                for (var i = 0; i < d.length; i++) {
                    var r = st.getById(d[i]);
                    if (r) {
                        var nv = r.data.val;
                        if (nv)
                            t.push(nv);
                    } else {
                        t.push(d[i]);
                    }
                }
                t.sort();
                v = t.join(',');
            }
            return v;
        };
    };

    this.editor = function(conf)
    {
        var cons = null;
        var combo = false;

        /* Editable? */
        var d = this.rdonly;
        if (this.wronly && !conf.create)
            d = false;

        /* Basic */
        var c = {
            fieldLabel: this.text,
            name: this.id,
            value: conf.value || null,
            disabled: d,
            width: 300
        };
        
        /* ComboBox */
        if (this['enum']) {

            c['valueField'] = 'key';
            c['displayField'] = 'val';

            if (this.list && this.lorder) {

                cons = Ext.ux.ItemSelector;
                c['fromStore'] = this.fromstore;
                c['toStore'] = this.tostore;
                c['fromSortField'] = 'val';
                c['dataFields'] = ['key', 'val'];
                c['msHeight'] =  150;
                c['imagePath'] = 'static/multiselect/resources',
                c['toLegend'] = _('Selected');
                c['fromLegend'] = _('Available');

            } else {
                cons = Ext.form.ComboBox;
                if (this.list) {
                    cons = Ext.ux.form.LovCombo;
                    c['checkField'] = 'checked_' + this.id;
                }

                /* Combo settings */
                c['mode'] = 'local';
                c['store'] = this.store;
                c['typeAhead'] = true;
                c['forceSelection'] = false;
                c['triggerAction'] = 'all';
                c['emptyText'] = _('Select {0} ...').replace('{0}', this.text);
                
                combo = true;
            }

            /* Single */
        } else {

            if (this.type == 'perm') {
                c['regex'] = /^[0][0-7]{3}$/;
                c['maskRe'] = /[0-7]/;
                c['allowBlank'] = false;
                c['blankText'] = _('You must provide a value - use octal chmod notation, e.g. 0664');
                c['width'] = 125;
            }

            switch (this.type) {
                case 'bool':
                    cons = Ext.form.Checkbox;
                    break;

                case 'int':
                case 'u32':
                case 'u16':
                case 's32':
                case 's64':
                case 'dbl':
                case 'time':
                    if (this.hexa) {
                        cons = Ext.form.TextField;
                    } else if (this.intsplit) {
                        c['maskRe'] = /[0-9\.]/;
                        cons = Ext.form.TextField;
                    } else
                        cons = Ext.form.NumberField;
                    break;

                /* 'str' and 'perm' */
                default:
                    cons = Ext.form.TextField;
                    break;
            }
        }

        var r = new cons(c);
        if (combo)
            r.doQuery = tvheadend.doQueryAnyMatch;
        return r;
    };
};

/*
 * IdNode
 */
tvheadend.IdNode = function(conf)
{
    /*
     * Properties
     */
    this.clazz = conf['class'];
    this.text = conf.caption || this.clazz;
    this.event = conf.event;
    this.props = conf.props;
    this.order = [];
    this.groups = conf.groups;
    this.fields = [];
    for (var i = 0; i < this.props.length; i++) {
        this.fields.push(new tvheadend.IdNodeField(this.props[i]));
    }
    var o = [];
    if (conf.order)
        o = conf.order.split(',');
    if (o) {
        while (o.length < this.fields.length)
            o.push(null);
        for (var i = 0; i < o.length; i++) {
            this.order[i] = null;
            if (o[i]) {
                for (var j = 0; j < this.fields.length; j++) {
                    if (this.fields[j].id === o[i]) {
                        this.order[i] = this.fields[j];
                        this.fields[j].ordered = true;
                        break;
                    }
                }
            }
        }
        for (var i = 0; i < o.length; i++) {
            if (this.order[i] == null) {
                for (var j = 0; j < this.fields.length; j++) {
                    if (!this.fields[j].ordered) {
                        this.fields[j].ordered = true;
                        this.order[i] = this.fields[j];
                        break;
                    }
                }
            }
        }
    }

    /*
     * Methods
     */

    this.length = function() {
        return this.fields.length;
    };

    this.field = function(index) {
        if (this.order)
            return this.order[index];
        else
            return this.fields[index];
    };
};

/*
 *
 */
tvheadend.idnode_uilevel_menu = function(uilevel, handler)
{
    var b;

    var text = function(uilevel) {
        if (uilevel === 'basic')
            return _('Basic');
        if (uilevel === 'advanced')
            return _('Advanced');
        return _('Expert');
    }

    function selected(m) {
        b.tvh_uilevel_set(m.initialConfig.tvh_uilevel, 1);
    }

    var m = new Ext.menu.Menu();
    m.add({
        text: _('Basic'),
        iconCls: 'uilevel_basic',
        tvh_uilevel: 'basic',
        handler: selected
    });
    m.add({
        text: _('Advanced'),
        iconCls: 'uilevel_advanced',
        tvh_uilevel: 'advanced',
        handler: selected
    });
    m.add({
        text: _('Expert'),
        iconCls: 'uilevel_expert',
        tvh_uilevel: 'expert',
        handler: selected
    });
    b = new Ext.Toolbar.Button({
        tooltip: _('Change the user interface level (basic, advanced, expert)'),
        iconCls: 'uilevel',
        text: _('View level') + ': ' + text(uilevel),
        menu: m,
        tvh_uilevel_set: function (l, refresh) {
            b.setText(_('View level: ') + text(l));
            handler(l, refresh);
        }
    });
    return b;
}

/*
 * Field editor
 */
tvheadend.idnode_editor_field = function(f, conf)
{
    var r = null;
    var d = f.rdonly || false;
    if (f.wronly && !conf.create)
        d = false;
    var value = f.value;
    if (value == null)
        value = f['default'];

    function postfield(r, f) {
        if (f.description && tvheadend.quicktips) {
            r.on('render', function(c) {
                 Ext.QuickTips.register({
                     target: c.getEl(),
                     text: f.description
                 });
                 Ext.QuickTips.register({
                     target: c.wrap,
                     text: f.description
                 });
                 Ext.QuickTips.register({
                     target: c.label,
                     text: f.description
                 });
            });
            r.on('beforedestroy', function(c) {
                 Ext.QuickTips.unregister(c.getEl());
                 Ext.QuickTips.unregister(c.wrap);
                 Ext.QuickTips.unregister(c.label);
            });
        }
        return r;
    }

    /* Ordered list */
    if (f['enum'] && f.lorder) {

        var st = tvheadend.idnode_enum_store(f);
        var fromst = tvheadend.idnode_enum_select_store(f, st, false);
        var tost = tvheadend.idnode_enum_select_store(f, st, true);
        r = new Ext.ux.ItemSelector({
            name: f.id,
            fromStore: fromst,
            toStore: tost,
            fromSortField: 'val',
            fieldLabel: f.caption,
            dataFields: ['key', 'val'],
            msWidth: 205,
            msHeight: 150,
            valueField: 'key',
            displayField: 'val',
            imagePath: 'static/multiselect/resources',
            toLegend: _('Selected'),
            fromLegend: _('Available')
        });

    /* Enumerated (combobox) type */
    } else if (f['enum']) {
        var cons = Ext.form.ComboBox;
        if (f.list)
            cons = Ext.ux.form.LovCombo;
        var st = tvheadend.idnode_enum_store(f);
        r = new cons({
            fieldLabel: f.caption,
            name: f.id,
            value: value,
            disabled: d,
            width: 300,
            mode: 'local',
            valueField: 'key',
            displayField: 'val',
            checkField: 'checked_' + f.id,
            store: st,
            typeAhead: true, // TODO: this does strange things in multi
            forceSelection: false,
            triggerAction: 'all',
            emptyText:  _('Select {0} ...').replace('{0}', f.caption),
            listeners: {
                beforequery: function(qe) {
                    delete qe.combo.lastQuery;
                }
            }
        });

        r.doQuery = tvheadend.doQueryAnyMatch;

        if (st.on) {
            var fn = function() {
                st.un('load', fn);
                r.setValue(value); // HACK: to get extjs to display properly
            };
            st.on('load', fn);
        }

        /* TODO: listeners for regexp?
         listeners       : { 
         keyup: function() {
         this.store.filter('val', this.getRawValue(), true, false);
         },
         beforequery: function(queryEvent) {
         queryEvent.combo.onLoad();
         // prevent doQuery from firing and clearing out my filter.
         return false; 
         }
         }
         */
    }

    if (r)
        return postfield(r, f);

    /* Singular */
    switch (f.type) {
        case 'bool':
            r = new Ext.ux.form.XCheckbox({
                fieldLabel: f.caption,
                name: f.id,
                checked: value,
                disabled: d
            });
            break;

        case 'time':
            if (!f.duration) {
                if (d) {
                    var dt = new Date(value * 1000);
                    value = f.date ? dt.toLocaleDateString() :
                                     dt.toLocaleString();
                    r = new Ext.form.TextField({
                        fieldLabel: f.caption,
                        name: f.id,
                        value: value,
                        disabled: true,
                        width: 300
                    });
                    break;
                }
                r = new Ext.ux.form.TwinDateTimeField({
                    fieldLabel: f.caption,
                    name: f.id,
                    value: value,
                    disabled: false,
                    width: 300,
                    timeFormat: 'H:i:s',
                    timeConfig: {
                        altFormats: 'H:i:s',
                        allowBlank: true,
                        increment: 10
                    },
                    dateFormat:'d.n.Y',
                    dateConfig: {
                        altFormats: 'Y-m-d|Y-n-d',
                        allowBlank: true
                    }
                });
                break;
            }
            /* fall thru!!! */

        case 'int':
        case 'u32':
        case 'u16':
        case 's64':
        case 'dbl':
            if (f.hexa) {
                r = new Ext.form.TextField({
                    fieldLabel: f.caption,
                    name: f.id,
                    value: '0x' + value.toString(16),
                    disabled: d,
                    width: 300,
                    maskRe: /[xX0-9a-fA-F\.]/
                });
                break;
            }
            if (f.intsplit) {
                /* this should be improved */
                r = new Ext.form.TextField({
                    fieldLabel: f.caption,
                    name: f.id,
                    value: value,
                    disabled: d,
                    width: 300,
                    maskRe: /[0-9\.]/
                });
                break;
            }
            r = new Ext.form.NumberField({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                width: 300
            });
            break;

        case 'perm':
            r = new Ext.form.TextField({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                width: 125,
                regex: /^[0][0-7]{3}$/,
                maskRe: /[0-7]/,
                allowBlank: false,
                blankText: _('You must provide a value - use octal chmod notation, e.g. 0664')
            });
            break;

        default:
            cons = f.multiline ? Ext.form.TextArea : Ext.form.TextField;
            r = new cons({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                inputType: f.password && !conf.showpwd ? 'password' : 'text',
                width: 300
            });

    }

    return postfield(r, f);
};

/*
 * ID node editor form fields
 */
tvheadend.idnode_editor_form = function(uilevel, d, meta, panel, conf)
{
    var af = [];
    var ef = [];
    var rf = [];
    var df = [];
    var groups = null;
    var width = 0;

    /* Fields */
    for (var i = 0; i < d.length; i++) {
        var p = d[i];
        if (conf.uuids && p.rdonly)
            continue;
        if (p.noui)
            continue;
        var f = tvheadend.idnode_editor_field(p, conf);
        if (!f)
            continue;
        if (conf.uuids) {
            var label = f.fieldLabel;
            f.fieldLabel = null;
            var w = 15 + 10 + conf.labelWidth + 10 +
                    (f.initialConfig.width ? f.initialConfig.width : 100) + 10;
            if (w > width)
                width = w;
            f = new Ext.form.CompositeField({
                items: [
                    new Ext.ux.form.XCheckbox({
                        width: 15,
                        height: 12,
                        name: '_x_' + f.name
                    }),
                    {
                        xtype: 'displayfield',
                        width: conf.labelWidth,
                        height: 12,
                        value: label
                    },
                    f
                ]
            });
        }
        if (p.group && meta && meta.groups) {
            f.tvh_uilevel = p.expert ? 'expert' : (p.advanced ? 'advanced' : 'basic');
            if (!groups)
                groups = {};
            if (!(p.group in groups))
                groups[p.group] = [f];
            else
                groups[p.group].push(f);
        } else {
            if (p.rdonly) {
                if (uilevel === 'expert' || (!p.advanced && !p.expert))
                    rf.push(f);
                else if (p.advanced && p.advanced)
                    rf.push(f);
            } else if (p.expert) {
                if (uilevel === 'expert')
                    ef.push(f);
            } else if (p.advanced) {
                if (uilevel !== 'basic')
                    af.push(f);
            } else {
                df.push(f);
            }
        }
    }

    function newFieldSet(conf) {
        return new Ext.form.FieldSet({
                       title: conf.title || '',
                       layout: conf.layout || 'form',
                       border: conf.border || true,
                       style: conf.style || 'padding: 0 5px 5px 10px',
                       bodyStyle: conf.bodyStyle || 'padding-top: ' + (Ext.isIE ? '0' : '10px'),
                       autoHeight: true,
                       autoWidth: true,
                       collapsible: conf.nocollapse ? false : true,
                       collapsed: conf.collapsed ? true : false,
                       animCollapse: true,
                       items: conf.items
                   });
    }

    if (groups) {
        var met = {};
        for (var i = 0; i < meta.groups.length; i++)
            met[meta.groups[i].number] = meta.groups[i];
        var fs = {};
        var cfs = {};
        var mfs = {};
        var rest = [];
        for (var number in groups) {
            if (!(number in met))
                met[number] = null;
            var m = met[number];
            var columns = 0;
            for (var k in met)
                if (met[k].parent == m.number)
                    if (columns < met[k].column)
                        columns = met[k].column;
            met[number].columns = columns;
            if (columns) {
                var p = newFieldSet({ title: m.name || _("Settings"), layout: 'column2', border: false });
                cfs[number] = newFieldSet({ nocollapse: true, style: 'border-width: 0px', bodyStyle: ' ' });
                p.add(cfs[number]);
                fs[number] = p;
                mfs[number] = p;
            }
        }
        for (var number in groups) {
            var m = met[number];
            if (number in fs) continue;
            var parent = m.parent;
            var p = null;
            if (parent && !met[parent].columns)
                parent = null;
            if (!m.columns) {
                if (parent) {
                    p = newFieldSet({ nocollapse: true, style: 'border-width: 0px', bodyStyle: ' ' });
                    fs[parent].add(p);
                } else {
                    p = newFieldSet({ title: m.name });
                    mfs[number] = p;
                }
                cfs[number] = p;                    
            }
        }
        for (var number in groups) {
            var g = groups[number];
            var cnt = 0;
            for (var i = 0; i < g.length; i++) {
                var f = g[i];
                if (!tvheadend.uilevel_match(f.tvh_uilevel, uilevel))
                    f.setVisible(false);
                else
                    cnt++;
                cfs[number].add(f);
            }
            if (!cnt)
                cfs[number].setVisible(false);
            if (number in mfs) {
                panel.add(mfs[number]);
                if (!cnt)
                    mfs[number].setVisible(false);
            }
        }
    }
    if (df.length && !af.length && !rf.length) {
        var f = newFieldSet({ nocollapse: true, items: df });
        panel.add(f);
    } else {
        if (df.length)
            panel.add(newFieldSet({ title: _("Basic Settings"), items: df }));
        if (af.length)
            panel.add(newFieldSet({ title: _("Advanced Settings"), items: af }));
        if (ef.length)
            panel.add(newFieldSet({ title: _("Expert Settings"), items: ef }));
        if (rf.length)
            panel.add(newFieldSet({ title: _("Read-only Info"), items: rf, collapsed: 'true'}));
    }
    panel.doLayout();
    if (width)
        panel.fixedWidth = width + 50;
    if (conf.uuids) {
        panel.getForm().getFieldValues = function(dirtyOnly) {
            o = {};
            this.items.each(function(f) {
                var cbox = f.items.itemAt(0);
                var field = f.items.itemAt(2);
                if (cbox.getValue())
                    o[field.getName()] = field.getValue();
            });
            return o;
        }
    }
};

/*
 * ID node editor panel
 */
tvheadend.idnode_editor = function(_uilevel, item, conf)
{
    var panel = null;
    var buttons = [];
    var uilevel = _uilevel;

    function destroy() {
        panel.removeAll(true);
    }
    
    function build() {
        var c = {
            showpwd: conf.showpwd,
            uuids: conf.uuids,
            labelWidth: conf.labelWidth || 200
        };

        tvheadend.idnode_editor_form(uilevel, item.props || item.params, item.meta, panel, c);
    }

    /* Buttons */
    if (!conf.noButtons) {
        if (conf.cancel) {
            var cancelBtn = new Ext.Button({
                text: _('Cancel'),
                iconCls: 'cancel',
                handler: function() {
                    conf.cancel(conf);
                }
            });
            buttons.push(cancelBtn);
        }

        var saveBtn = new Ext.Button({
            text: conf.saveText || _('Save'),
            iconCls: conf.saveIconCls || 'save',
            handler: function() {
                var node = null;
                if (panel.getForm().isDirty() || conf.alwaysDirty) {
                    node = panel.getForm().getFieldValues();
                    node.uuid = conf.uuids ? conf.uuids : item.uuid;
                    tvheadend.Ajax({
                        url: conf.saveURL || 'api/idnode/save',
                        params: {
                            node: Ext.encode(node)
                        },
                        success: function(d) {
                            if (conf.win)
                                conf.win.close();
                            if (conf.postsave)
                                conf.postsave(conf, node);
                        }
                    });
                } else {
                    if (conf.win)
                        conf.win.close();
                    if (conf.postsave)
                        conf.postsave(conf, node);
                }
            }
        });
        buttons.push(saveBtn);

        if (!conf.noApply) {
            var applyBtn = new Ext.Button({
                text: _('Apply'),
                iconCls: 'apply',
                handler: function() {
                    if (panel.getForm().isDirty()) {
                        var node = panel.getForm().getFieldValues();
                        node.uuid = conf.uuids ? conf.uuids : item.uuid;
                        tvheadend.Ajax({
                            url: conf.saveURL || 'api/idnode/save',
                            params: {
                                node: Ext.encode(node)
                            },
                            success: function(d) {
                                panel.getForm().reset();
                            }
                        });
                    }
                }
            });
            buttons.push(applyBtn);
        }


        var uilevelBtn = null;
        if (!tvheadend.uilevel_nochange &&
            (!conf.uilevel || conf.uilevel !== 'expert') &&
            !conf.noUIlevel) {
            uilevelBtn = tvheadend.idnode_uilevel_menu(uilevel, function(l) {
                uilevel = l;
                var values = panel.getForm().getFieldValues();
                destroy();
                build();
                panel.getForm().setValues(values);
            });
            buttons.push('->');
            buttons.push(uilevelBtn);
        }

        if (conf.help) {
            var helpBtn = new Ext.Button({
                text: _('Help'),
                iconCls: 'help',
                handler: conf.help
            });
            buttons.push(uilevelBtn ? '-' : '->');
            buttons.push(helpBtn);
        }

        if (conf.buildbtn)
            conf.buildbtn(conf, buttons);
    }

    panel = new Ext.form.FormPanel({
        title: conf.title || null,
        frame: true,
        border: conf.inTabPanel ? false : true,
        bodyStyle: 'padding: 5px',
        labelAlign: 'left',
        labelWidth: conf.uuids ? 1 : (conf.labelWidth || 200),
        autoWidth: conf.noautoWidth ? false : true,
        autoHeight: !conf.fixedHeight,
        width: conf.nowidth ? null : (conf.width || 600),
        defaultType: 'textfield',
        buttonAlign: 'left',
        autoScroll: true,
        buttons: buttons
    });

    build();
    if (conf.build)
        conf.build(conf, panel);
    return panel;
};

/*
 *
 */
tvheadend.idnode_editor_win = function(_uilevel, conf)
{
    function display(item, conf, title) {
        var p = tvheadend.idnode_editor(_uilevel, item, conf);
        var width = p.fixedWidth;
        var w = new Ext.ux.Window({
            title: title,
            iconCls: conf.iconCls || 'edit',
            layout: 'fit',
            autoWidth: width ? false : true,
            autoHeight: true,
            autoScroll: true,
            plain: true,
            items: p
        });
        conf.win = w;
        if (width)
            w.setWidth(width);
        w.show();
        return w;
    }

    if (!conf.cancel)
        conf.cancel = function(conf) {
            conf.win.close();
            conf.win = null;
       }

    if (conf.fullData) {
       display(conf.fullData, conf, conf.winTitle || _('Edit'));
       return;
    }

    var params = conf.params || {};

    var uuids = null;
    if (conf.selections) {
        var r = conf.selections;
        if (!r || r.length <= 0)
            return;

        uuids = [];
        for (var i = 0; i < r.length; i++)
            uuids.push(r[i].id);
            
        params['uuid'] = r[0].id;
    }
        
    params['meta'] = 1;

    conf.win = null;

    tvheadend.Ajax({
        url: conf.loadURL ? conf.loadURL : 'api/idnode/load',
        params: params,
        success: function(d) {
            d = json_decode(d);
            d = d[0];
            if (conf.modifyData)
                conf.modifyData(conf, d);
            var title = conf.winTitle;
            if (!title) {
                if (uuids && uuids.length > 1)
                    title = String.format(_('Edit {0} ({1} entries)'),
                                              conf.titleS, uuids.length);
                else
                    title = String.format(_('Edit {0}'), conf.titleS);
            }
            if (uuids && uuids.length > 1)
                conf.uuids = uuids;
            display(d, conf, title);
        }
    });
}

/*
 * IDnode creation dialog
 */
tvheadend.idnode_create = function(conf, onlyDefault, cloneValues)
{
    var puuid = null;
    var panel = null;
    var win   = null;
    var pclass = null;
    var buttons = [];
    var abuttons = {};
    var uilevel = tvheadend.uilevel;
    var values = null;

    function doselect() {
        var store = conf.select.store;
        if (!store) {
            store = new Ext.data.JsonStore({
                root: 'entries',
                url: conf.select.url || conf.url,
                baseParams: conf.select.params,
                fields: [conf.select.valueField, conf.select.displayField]
            });
        }
        var select = null;
        if (conf.select.fullRecord) {
            select = function(s, n, o) {
                var r = store.getAt(s.selectedIndex);
                if (r) {
                    var d = r.json.props;
                    if (d) {
                        d = tvheadend.idnode_filter_fields(d, conf.select.list || null);
                        pclass = r.get(conf.select.valueField);
                        win.setTitle(String.format(_('Add {0}'), s.lastSelectionText));
                        panel.remove(s);
                        tvheadend.idnode_editor_form(uilevel, d, r.json, panel, { create: true, showpwd: true });
                        abuttons.save.setVisible(true);
                        abuttons.apply.setVisible(true);
                        win.setOriginSize(true);
                        if (values)
                            panel.getForm().setValues(values);
                    }
                }
            };
        } else {
            select = function(s, n, o) {
                params = conf.select.clazz.params || {};
                params['uuid'] = puuid = n.id;
                tvheadend.Ajax({
                    url: conf.select.clazz.url || conf.select.url || conf.url,
                    params: params,
                    success: function(d) {
                        panel.remove(s);
                        d = json_decode(d);
                        tvheadend.idnode_editor_form(uilevel, d.props, d, panel, { create: true, showpwd: true });
                        abuttons.save.setVisible(true);
                        abuttons.apply.setVisible(true);
                        win.setOriginSize(true);
                        if (values)
                            panel.getForm().setValues(values);
                    }
                });
            };
        }

        /* Parent selector */
        var combo = new Ext.form.ComboBox({
            fieldLabel: conf.select.label,
            grow: true,
            editable: false,
            allowBlank: false,
            displayField: conf.select.displayField,
            valueField: conf.select.valueField,
            mode: 'remote',
            triggerAction: 'all',
            store: store,
            listeners: {
                select: select
            }
        });

        panel.add(combo);
        win.show();
    }

    function dodirect() {
        tvheadend.Ajax({
            url: conf.url + '/class',
            params: conf.params,
            success: function(d) {
                d = json_decode(d);
                tvheadend.idnode_editor_form(uilevel, d.props, d, panel, { create: true, showpwd: true });
                if (onlyDefault) {
                    if (cloneValues)
                        panel.getForm().setValues(cloneValues);
                    conf.forceSave = true;
                    abuttons.save.handler();
                    delete conf.forceSave;
                    panel.destroy();
                    if (cloneValues)
                        Ext.MessageBox.alert(_('Clone'), _('The selected entry is the original!'));
                } else {
                    abuttons.save.setVisible(true);
                    abuttons.apply.setVisible(true);
                    if (values)
                        panel.getForm().setValues(values);
                    win.show();
                }
            }
        });
    }

    function createwin() {

        /* Close previous window */
        if (win) {
            win.close();
            win = null;
            panel = null;
            abuttons = {};
            buttons = [];
        }

        /* Buttons */
        abuttons.save = new Ext.Button({
            tooltip: _('Create new entry'),
            text: _('Create'),
            iconCls: 'add',
            hidden: true,
            handler: function() {
                if (panel.getForm().isDirty() || conf.forceSave) {
                    var params = conf.create.params || {};
                    if (puuid)
                        params['uuid'] = puuid;
                    if (pclass)
                        params['class'] = pclass;
                    params['conf'] = Ext.encode(panel.getForm().getFieldValues());
                    tvheadend.Ajax({
                        url: conf.create.url || conf.url + '/create',
                        params: params,
                        success: function(d) {
                            win.close();
                        }
                    });
                } else {
                    win.close();
                }
            }
        });
        buttons.push(abuttons.save);
        
        abuttons.apply = new Ext.Button({
            tooltip: _('Apply settings'),
            text: _('Apply'),
            iconCls: 'apply',
            hidden: true,
            handler: function() {
                if (panel.getForm().isDirty()) {
                    var params = conf.create.params || {};
                    if (puuid)
                        params['uuid'] = puuid;
                    if (pclass)
                        params['class'] = pclass;
                    params['conf'] = Ext.encode(panel.getForm().getFieldValues());
                    tvheadend.Ajax({
                        url: conf.create.url || conf.url + '/create',
                        params: params,
                        success: function(d) {
                            panel.getForm().reset();
                        }
                    });
                }
            }
        });
        buttons.push(abuttons.apply);

        abuttons.cancel = new Ext.Button({
            tooltip: _('Cancel operation'),
            text: _('Cancel'),
            iconCls: 'cancelButton',
            handler: function() {
                win.close();
                win = null;
            }
        });
        buttons.push(abuttons.cancel);

        if (!tvheadend.uilevel_nochange && (!conf.uilevel || conf.uilevel !== 'expert')) {
            abuttons.uilevel = tvheadend.idnode_uilevel_menu(uilevel, function (l) {
                values = panel.getForm().getFieldValues();
                uilevel = l;
                createwin();
            });
            buttons.push('->');
            buttons.push(abuttons.uilevel);
        }

        /* Form */
        panel = new Ext.FormPanel({
            frame: true,
            border: true,
            bodyStyle: 'padding: 5px',
            labelAlign: 'left',
            labelWidth: 200,
            autoWidth: true,
            autoHeight: true,
            defaultType: 'textfield',
            buttonAlign: 'left',
            items: [],
            buttons: buttons
        });

        /* Create window */
        win = new Ext.ux.Window({
            title: String.format(_('Add {0}'), conf.titleS),
            iconCls: 'add',
            layout: 'fit',
            autoWidth: true,
            autoHeight: true,
            autoScroll: true,
            plain: true,
            items: panel
        });

        /* Do we need to first select a class? */
        if (conf.select) {
            doselect();
        } else {
            dodirect();
        }
    }

    createwin();
};

/*
 *
 */
tvheadend.idnode_panel = function(conf, panel, dpanel, builder, destroyer)
{
    if (!conf.uilevel || tvheadend.uilevel_match(conf.uilevel, tvheadend.uilevel)) {
        tvheadend.paneladd(panel, dpanel, conf.tabIndex);
        tvheadend.panelreg(panel, dpanel, builder, destroyer);
    }
}

/*
 * IDnode grid
 */
tvheadend.idnode_grid = function(panel, conf)
{
    var store = null;
    var grid = null;
    var event = null;
    var auto = null;
    var idnode = null;

    var update = function(o) {
        if ((o.create || o.moveup || o.movedown || 'delete' in o) && auto.getValue()) {
            store.reload();
            return;
        }
        if ('delete' in o) {
            Ext.each(o['delete'], function (d) {
                var r = store.getById(d);
                if (r)
                    store.remove(r);
            });
        }
        if (o.change) {
            var ids = [];
            Ext.each(o.change, function(id) {
                var r = store.getById(id);
                if (r)
                    ids.push(r.id);
            });
            if (ids) {
                var p = { uuid: Ext.encode(ids), grid: 1 };
                if (conf.list) p.list = conf.list;
                Ext.Ajax.request({
                    url: 'api/idnode/load',
                    params: p,
                    success: function (d) {
                        d = json_decode(d);
                        Ext.each(d, function(jd) {
                            tvheadend.replace_entry(store.getById(jd.uuid), jd);
                        });
                    },
                    failure: function(response, options) {
                        Ext.MessageBox.alert(_('Grid Update'), response.statusText);
                    }
                });
            }
        }
    };

    var update2 = function(o) {
        if (grid)
            grid.getView().refresh();
    };

    function build(d) {
        if (grid)
            return;

        if (conf.builder)
            conf.builder(conf);

        var columns = [];
        var filters = [];
        var fields = [];
        var ifields = [];
        var buttons = [];
        var abuttons = {};
        var plugins = conf.plugins ? conf.plugins.slice() : [];
        var uilevel = tvheadend.uilevel;

        /* Some copies */
        if (conf.add && !conf.add.titleS && conf.titleS)
            conf.add.titleS = conf.titleS;

        /* Left-hand columns (do copy, no reference!) */
        if (conf.lcol)
            for (i = 0; i < conf.lcol.length; i++)
                columns.push(conf.lcol[i]);

        /* Model */
        idnode = new tvheadend.IdNode(d);
        for (var i = 0; i < idnode.length(); i++) {
            var f = idnode.field(i);
            var c = f.column(uilevel, conf.columns);
            fields.push(f.id);
            ifields.push(f);
            if (!f.noui) {
                c['tooltip'] = f.description || f.text;
                columns.push(c);
                if (c.filter)
                    filters.push(c.filter);
                f.onrefresh(update2);
            }
        }

        /* Right-hand columns */
        if (conf.rcol)
            for (i = 0; i < conf.rcol.length; i++)
               columns.push(conf.rcol[i]);

        /* Filters */
        var filter = new Ext.ux.grid.GridFilters({
            encode: true,
            local: false,
            filters: filters
        });

        /* Store */
        var params = {};
        if (conf.all) params['all'] = 1;
        store = new Ext.data.JsonStore({
            root: 'entries',
            url: conf.gridURL || (conf.url + '/grid'),
            baseParams: params,
            autoLoad: true,
            id: 'uuid',
            totalProperty: 'total',
            fields: fields,
            remoteSort: true,
            pruneModifiedRecords: true,
            sortInfo: conf.sort ? conf.sort : null
        });

        /* Model */
        var model = new Ext.grid.ColumnModel({
            defaultSortable: conf.move ? false : true,
            columns: columns
        });

        /* Selection */
        var select = new Ext.grid.RowSelectionModel({
            singleSelect: false
        });

        /* Event handlers */
        store.on('update', function(s, r, o) {
            var d = (s.getModifiedRecords().length === 0);
            if (abuttons.undo)
                abuttons.undo.setDisabled(d);
            if (abuttons.save)
                abuttons.save.setDisabled(d);
        });
        select.on('selectionchange', function(s) {
            var count = s.getCount();
            if (abuttons.del)
                abuttons.del.setDisabled(count === 0);
            if (abuttons.up) {
                abuttons.up.setDisabled(count === 0);
                abuttons.down.setDisabled(count === 0);
            }
            if (abuttons.edit)
                abuttons.edit.setDisabled(count === 0);
            if (conf.selected)
                conf.selected(s, abuttons);
        });

        /* Top bar */
        if (!conf.readonly) {
            abuttons.save = new Ext.Toolbar.Button({
                tooltip: _('Save pending changes (marked with red border)'),
                iconCls: 'save',
                text: _('Save'),
                disabled: true,
                handler: function() {
                    var mr = store.getModifiedRecords();
                    var out = new Array();
                    for (var x = 0; x < mr.length; x++) {
                        v = mr[x].getChanges();
                        out[x] = v;
                        out[x].uuid = mr[x].id;
                    }
                    tvheadend.Ajax({
                        url: 'api/idnode/save',
                        params: {
                            node: Ext.encode(out)
                        },
                        success: function(d)
                        {
                            if (!auto.getValue())
                                store.reload();
                        }
                    });
                }
            });
            buttons.push(abuttons.save);
            abuttons.undo = new Ext.Toolbar.Button({
                tooltip: _('Revert pending changes (marked with red border)'),
                iconCls: 'undo',
                text: _('Undo'),
                disabled: true,
                handler: function() {
                    store.rejectChanges();
                }
            });
            buttons.push(abuttons.undo);
        }
        if (conf.add) {
            if (buttons.length > 0)
                buttons.push('-');
            abuttons.add = new Ext.Toolbar.Button({
                tooltip: _('Add a new entry'),
                iconCls: 'add',
                text: _('Add'),
                disabled: false,
                handler: function() {
                    tvheadend.idnode_create(conf.add);
                }
            });
            buttons.push(abuttons.add);
        }
        if (conf.del) {
            if (!conf.add && buttons.length > 0)
                buttons.push('-');
            abuttons.del = new Ext.Toolbar.Button({
                tooltip: _('Delete selected entries'),
                iconCls: 'remove',
                text: _('Delete'),
                disabled: true,
                handler: function() {
                    var r = select.getSelections();
                    if (r && r.length > 0) {
                        var uuids = [];
                        for (var i = 0; i < r.length; i++)
                            uuids.push(r[i].id);
                        c = {
                            url: 'api/idnode/delete',
                            params: {
                                uuid: Ext.encode(uuids)
                            },
                            success: function(d)
                            {
                                if (!auto.getValue())
                                    store.reload();
                            }
                        };
                        if (conf.delquestion)
                            c['question'] = conf.delquestion;
                        tvheadend.AjaxConfirm(c);
                    }
                }
            });
            buttons.push(abuttons.del);
        }
        if (conf.move) {
            abuttons.up = new Ext.Toolbar.Button({
                tooltip: _('Move selected entries up'),
                iconCls: 'moveup',
                text: _('Move Up'),
                disabled: true,
                handler: function() {
                    var r = select.getSelections();
                    if (r && r.length > 0) {
                        var uuids = [];
                        for (var i = 0; i < r.length; i++)
                            uuids.push(r[i].id);
                        tvheadend.Ajax({
                            url: 'api/idnode/moveup',
                            params: {
                                uuid: Ext.encode(uuids)
                            },
                            success: function(d)
                            {
                                store.reload();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.up);
            abuttons.down = new Ext.Toolbar.Button({
                tooltip: _('Move selected entries down'),
                iconCls: 'movedown',
                text: _('Move Down'),
                disabled: true,
                handler: function() {
                    var r = select.getSelections();
                    if (r && r.length > 0) {
                        var uuids = [];
                        for (var i = 0; i < r.length; i++)
                            uuids.push(r[i].id);
                        tvheadend.Ajax({
                            url: 'api/idnode/movedown',
                            params: {
                                uuid: Ext.encode(uuids)
                            },
                            success: function(d)
                            {
                                store.reload();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.down);
        }
        if (!conf.readonly) {
            if (buttons.length > 0)
                buttons.push('-');
        }
        if (!conf.readonly || conf.edit) {
            abuttons.edit = new Ext.Toolbar.Button({
                tooltip: _('Edit selected entry'),
                iconCls: 'edit',
                text: _('Edit'),
                disabled: true,
                handler: function() {
                    if (conf.edittree) {
                        var r = select.getSelected();
                        if (r) {
                            var p = tvheadend.idnode_tree({
                                url: 'api/idnode/tree',
                                params: {
                                    root: r.id
                                }
                            });
                            p.setSize(800, 600);
                            var w = new Ext.Window({
                                title: String.format(_('Edit {0}'), conf.titleS),
                                iconCls: 'edit',
                                layout: 'fit',
                                autoWidth: true,
                                autoHeight: true,
                                plain: true,
                                items: p
                            });
                            w.show();
                        }
                    } else {
                        tvheadend.idnode_editor_win(uilevel, {
                            selections: select.getSelections(),
                            params: conf.edit && conf.edit.params ? conf.edit.params : null
                        });
                    }
                }
            });
            buttons.push(abuttons.edit);
        }

        /* Hide Mode */
        if (conf.hidemode) {
            var hidemode = new Ext.form.ComboBox({
                width: 100,
                displayField: 'val',
                valueField: 'key',
                store: new Ext.data.ArrayStore({
                    id: 0,
                    fields: ['key', 'val'],
                    data: [['default', _('Parent disabled')],
                        ['all', _('All')],
                        ['none', _('None')]]
                }),
                value: 'default',
                mode: 'local',
                forceSelection: false,
                triggerAction: 'all',
                listeners: {
                    select: function(s, r) {
                        store.baseParams.hidemode = r.id;
                        page.changePage(0);
                        store.reload();
                    }
                }
            });
            buttons.push('-');
            buttons.push(_('Hide') + ':');
            buttons.push(hidemode);
        }

        /* Grid Panel */
        auto = new Ext.form.Checkbox({
            checked: true,
            listeners: {
                check: function(s, c) {
                    if (c)
                        store.reload();
                }
            }
        });

        var count = new Ext.form.ComboBox({
            width: 50,
            displayField: 'val',
            valueField: 'key',
            store: new Ext.data.ArrayStore({
                id: 0,
                fields: ['key', 'val'],
                data: [[25, '25'], [50, '50'], [100, '100'],
                    [200, '200'], [999999999, _('All')]]
            }),
            value: 50,
            mode: 'local',
            forceSelection: false,
            triggerAction: 'all',
            listeners: {
                select: function(s, r) {
                    if (r !== page.pageSize) {
                        page.pageSize = r.id;
                        page.changePage(0);
                        store.reload();
                        // TODO: currently setting pageSize=-1 to disable paging confuses
                        //       the UI elements, and I don't know how to sort that!
                    }
                }
            }
        });

        var page = new Ext.PagingToolbar(
            tvheadend.PagingToolbarConf({store:store}, conf.titleP, auto, count)
        );

        /* Extra buttons */
        if (conf.tbar) {
            buttons.push('-');
            for (i = 0; i < conf.tbar.length; i++) {
                var t = conf.tbar[i];
                if (t.name && t.builder) {
                    var b = t.builder();
                    if (b.menu) {
                        b.menu.items.each(function(mi) {
                            mi.callback = t.callback[mi.name];
                            mi.setHandler(function(m, e) {
                                this.callback(this, e, store, select);
                            });
                        });
                    } else if (t.callback) {
                        b.callback = t.callback;
                        b.handler = function(b, e) {
                            this.callback(this, e, store, select);
                        }
                    }
                    abuttons[t.name] = b;
                    buttons.push(b);
                } else if (t.name)
                    buttons.push(t.name);
            }
        }

        if (!tvheadend.uilevel_nochange && (!conf.uilevel || conf.uilevel !== 'expert')) {
            abuttons.uilevel = tvheadend.idnode_uilevel_menu(uilevel, function (l) {
                uilevel = l;
                for (var i = 0; i < ifields.length; i++)
                    if (!ifields[i].noui) {
                        var h = ifields[i].get_hidden(uilevel);
                        model.setHidden(model.findColumnIndex(ifields[i].id), h);
                    }
            });
            buttons.push('->');
            buttons.push(abuttons.uilevel);
        }

        /* Help */
        if (conf.help) {
            buttons.push(abuttons.uilevel ? '-' : '->');
            buttons.push({
                text: _('Help'),
                iconCls: 'help',
                handler: conf.help
            });
        }

        plugins.push(filter);
        var gconf = {
            stateful: true,
            stateId: conf.gridURL || conf.url,
            stripeRows: true,
            store: store,
            cm: model,
            selModel: select,
            plugins: plugins,
            viewConfig: {
                forceFit: true
            },
            keys: {
                key: 'a',
                ctrl: true,
                stopEvent: true,
                handler: function() {
                    grid.getSelectionModel().selectAll();
                }
            },
            tbar: buttons,
            bbar: page
        };
        grid = conf.readonly ? new Ext.grid.GridPanel(gconf) :
                               new Ext.grid.EditorGridPanel(gconf);
        grid.on('filterupdate', function() {
            page.changePage(0);
        });
        if (conf.beforeedit)
          grid.on('beforeedit', conf.beforeedit);

        dpanel.add(grid);
        dpanel.doLayout(false, true);

        /* Add comet listeners */
        if (conf.comet)
            tvheadend.comet.on(conf.comet, update);
        if (idnode.event && idnode.event != conf.comet) {
            event = idnode.event;
            tvheadend.comet.on(idnode.event, update);
        }
    }

    function builder() {
        if (grid)
            return;

        /* Request data */
        if (!conf.fields) {
            var p = {};
            if (conf.list) p['list'] = conf.list;
            if (conf.all) p['all'] = 1;
            tvheadend.Ajax({
                url: conf.url + '/class',
                params: p,
                success: function(d)
                {
                    var d = json_decode(d);
                    build(d);
                }
            });
        } else {
            build(conf.fields);
        }
    }

    function destroyer() {
        if (grid === null || !tvheadend.dynamic)
            return;
        if (conf.comet)
            tvheadend.comet.un(conf.comet, update);
        if (event)
            tvheadend.comet.un(event, update);
        for (var i = 0; i < idnode.length(); i++) {
            var f = idnode.field(i);
            f.unrefresh();
        }
        dpanel.removeAll(true);
        store.destroy();
        grid = null;
        store = null;
        auto = null;
        event = null;
        idnode = null;
        if (conf.destroyer)
            conf.destroyer(conf);
    }

    var dpanel = new Ext.Panel({
        id: conf.id || null,
        border: false,
        header: false,
        layout: 'fit',
        title: conf.titleP || '',
        iconCls: conf.iconCls || ''
    });

    tvheadend.idnode_panel(conf, panel, dpanel, builder, destroyer);
};

/*
 * IDnode form grid
 */
tvheadend.idnode_form_grid = function(panel, conf)
{
    var mpanel = null;
    var store = null;

    var update = function(o) {
        if (store)
            store.reload();
    };

    function builder() {
        if (mpanel)
            return;

        if (conf.builder)
            conf.builder(conf);

        var columns = [];
        var buttons = [];
        var abuttons = {};
        var plugins = conf.plugins ? conf.plugins.slice() : [];
        var current = null;
        var selectuuid = null;
        var uilevel = tvheadend.uilevel;

        /* Store */
        var listurl = conf.list ? conf.list.url : null;
        var params = conf.list ? conf.list.params : null;
        store = new Ext.data.JsonStore({
            root: 'entries',
            url: listurl || 'api/idnode/load',
            baseParams: params || {
                'enum': 1,
                'class': conf.clazz
            },
            autoLoad: true,
            id: conf.key || 'key',
            totalProperty: 'total',
            fields: conf.fields || ['key','val'],
            remoteSort: false,
            pruneModifiedRecords: true,
            sortInfo: conf.move ? conf.sort :
                      (conf.sort || { field: conf.val || 'val', direction: 'ASC' })
        });

        store.on('load', function(records) {
            var s = false;
            if (selectuuid) {
                records.each(function(r) {
                    if (r.id === selectuuid) {
                        select.selectRecords([r]);
                        s = true;
                    }
                });
                selectuuid = null;
            } else if (!current && !select.getSelected())
                select.selectFirstRow();
        });

        /* Left-hand columns (do copy, no reference!) */
        if (conf.lcol)
            for (i = 0; i < conf.lcol.length; i++)
                columns.push(conf.lcol[i]);

        columns.push({
            width: 300,
            id: conf.val || 'val',
            header: conf.titleC,
            sortable: conf.move ? false : true,
            dataIndex: conf.val || 'val'
        });

        /* Model */
        var model = new Ext.grid.ColumnModel({
            defaultSortable: conf.move ? false : true,
            columns: columns
        });

        /* Selection */
        var select = new Ext.grid.RowSelectionModel({
            singleSelect: true
        });

        /* Event handlers */
        select.on('selectionchange', function(s) {
            roweditor(s.getSelected());
            if (conf.selected)
                conf.selected(s, abuttons);
        });

        /* Top bar */
        abuttons.save = new Ext.Toolbar.Button({
            tooltip: _('Save pending changes (marked with red border)'),
            iconCls: 'save',
            text: _('Save'),
            disabled: true,
            handler: function() {
                var node = current.editor.getForm().getFieldValues();
                node.uuid = current.uuid;
                tvheadend.Ajax({
                    url: 'api/idnode/save',
                    params: {
                        node: Ext.encode(node)
                    },
                    success: function() {
                        selectuuid = current.uuid;
                        roweditor_destroy();
                        store.reload();
                    }
                });
            }
        });
        buttons.push(abuttons.save);
        abuttons.undo = new Ext.Toolbar.Button({
            tooltip: _('Revert pending changes (marked with red border)'),
            iconCls: 'undo',
            text: _('Undo'),
            disabled: true,
            handler: function() {
                if (current)
                    current.editor.getForm().reset();
            }
        });
        buttons.push(abuttons.undo);
        buttons.push('-');
        if (conf.add) {
            abuttons.add = new Ext.Toolbar.Button({
                tooltip: _('Add a new entry'),
                iconCls: 'add',
                text: _('Add'),
                disabled: false,
                handler: function() {
                    tvheadend.idnode_create(conf.add, true);
                }
            });
            buttons.push(abuttons.add);
            abuttons.clone = new Ext.Toolbar.Button({
                tooltip: _('Clone a new entry'),
                iconCls: 'clone',
                text: _('Clone'),
                disabled: false,
                handler: function() {
                    if (current)
                        tvheadend.idnode_create(conf.add, true, current.editor.getForm().getFieldValues());
                }
            });
            buttons.push(abuttons.clone);
        }
        if (conf.del) {
            abuttons.del = new Ext.Toolbar.Button({
                tooltip: _('Delete selected entries'),
                iconCls: 'remove',
                text: _('Delete'),
                disabled: true,
                handler: function() {
                    if (current) {
                        var c = {
                            url: 'api/idnode/delete',
                            params: {
                                uuid: current.uuid
                            },
                            success: function(d) {
                                roweditor_destroy();
                                store.reload();
                            }
                        };
                        if (conf.delquestion)
                            c['delquestion'] = conf.delquestion;
                        tvheadend.AjaxConfirm(c);
                    }
                }
            });
            buttons.push(abuttons.del);
        }
        if (conf.move) {
            abuttons.up = new Ext.Toolbar.Button({
                tooltip: _('Move selected entry up'),
                iconCls: 'moveup',
                text: _('Move Up'),
                disabled: true,
                handler: function() {
                    if (current) {
                        tvheadend.Ajax({
                            url: 'api/idnode/moveup',
                            params: {
                                uuid: current.uuid
                            },
                            success: function(d)
                            {
                                store.reload();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.up);
            abuttons.down = new Ext.Toolbar.Button({
                tooltip: _('Move selected entry down'),
                iconCls: 'movedown',
                text: _('Move Down'),
                disabled: true,
                handler: function() {
                    if (current) {
                        tvheadend.Ajax({
                            url: 'api/idnode/movedown',
                            params: {
                                uuid: current.uuid
                            },
                            success: function(d)
                            {
                                store.reload();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.down);
        }
        if (conf.hidepwd) {
            buttons.push('-');
            abuttons.add = new Ext.Toolbar.Button({
                tooltip: _('Show or hide passwords'),
                iconCls: 'eye',
                text: _('Show passwords'),
                disabled: false,
                handler: function() {
                    conf.showpwd = !conf.showpwd ? true : false;
                    this.setText(conf.showpwd ? _('Hide passwords') : _('Show passwords'));
                    roweditor_destroy();
                    roweditor(select.getSelected());
                }
            });
            buttons.push(abuttons.add);
        }

        /* Extra buttons */
        if (conf.tbar) {
            buttons.push('-');
            for (i = 0; i < conf.tbar.length; i++) {
                var t = conf.tbar[i];
                if (t.name && t.builder) {
                    var b = t.builder();
                    if (t.callback) {
                        b.callback = t.callback;
                        b.handler = function(b, e) {
                            this.callback(this, e, store, select);
                        }
                    }
                    abuttons[t.name] = b;
                    buttons.push(b);
                } else if (t.name)
                    buttons.push(t.name);
            }
        }

        if (!tvheadend.uilevel_nochange && (!conf.uilevel || conf.uilevel !== 'expert')) {
            abuttons.uilevel = tvheadend.idnode_uilevel_menu(uilevel, function (l) {
                uilevel = l;
                var values = null;
                if (current)
                    values = current.editor.getForm().getFieldValues();
                roweditor_destroy();
                roweditor(select.getSelected());
                if (values && current)
                    current.editor.getForm().setValues(values);
            });
            buttons.push('->');
            buttons.push(abuttons.uilevel);
        }

        /* Help */
        if (conf.help) {
            buttons.push(abuttons.uilevel ? '-' : '->');
            buttons.push({
                text: _('Help'),
                iconCls: 'help',
                handler: conf.help
            });
        }

        function roweditor_destroy() {
            if (current && current.editor) {
                mpanel.remove(current.editor);
                current.editor.destroy();
            }
            current = null;
        }

        function roweditor(r, force) {
            if (!r || !r.id)
                return;
            if (!force && current && current.uuid == r.id)
                return;
            var params = conf.edit ? (conf.edit.params || {}) : {};
            params.uuid = r.id;
            params.meta = 1;
            tvheadend.Ajax({
                url: 'api/idnode/load',
                params: params,
                success: function(d) {
                    d = json_decode(d);
                    roweditor_destroy();
                    current = new Object();
                    current.uuid = d[0].id;
                    current.editor = new tvheadend.idnode_editor(uilevel, d[0], {
                        title: _('Parameters'),
                        labelWidth: 300,
                        fixedHeight: true,
                        help: conf.help || null,
                        inTabPanel: true,
                        noButtons: true,
                        width: 730,
                        noautoWidth: true,
                        showpwd: conf.showpwd
                    });
                    abuttons.save.setDisabled(false);
                    abuttons.undo.setDisabled(false);
                    if (abuttons.del)
                      abuttons.del.setDisabled(false);
                    if (abuttons.up) {
                      abuttons.up.setDisabled(false);
                      abuttons.down.setDisabled(false);
                    }
                    mpanel.add(current.editor);
                    mpanel.doLayout();
                }
            });
        }

        /* Grid Panel (Selector) */
        var grid = new Ext.grid.GridPanel({
            flex: 1,
            autoWidth: true,
            autoScroll: true,
            stripeRows: true,
            store: store,
            cm: model,
            selModel: select,
            plugins: plugins,
            border: false,
            viewConfig: {
                forceFit: true
            },
            listeners : {
                render : {
                    fn :  function() {
                        if (!current)
                            select.selectFirstRow();
                    }
                }
            }
        });

        mpanel = new Ext.Panel({
            tbar: buttons,
            layout: 'hbox',
            padding: 5,
            border: false,
            layoutConfig: {
               align: 'stretch'
            },
            items: [grid]
        });
        
        dpanel.add(mpanel);
        dpanel.doLayout(false, true);

        if (conf.comet)
            tvheadend.comet.on(conf.comet, update);
    }

    function destroyer() {
        if (mpanel === null || !tvheadend.dynamic)
            return;
        if (conf.comet)
            tvheadend.comet.un(conf.comet, update);
        mpanel = null;
        dpanel.removeAll(true);
        store.destroy();
        store = null;
        if (conf.destroyer)
            conf.destroyer(conf);
    }

    var dpanel = new Ext.Panel({
        border: false,
        header: false,
        layout: 'fit',
        title: conf.titleP || '',
        iconCls: conf.iconCls || ''
    });

    tvheadend.idnode_panel(conf, panel, dpanel, builder, destroyer);
};

/*
 * IDNode Tree
 */
tvheadend.idnode_tree = function(panel, conf)
{
    var tree = null;
    var events = {};

    function update(o) {
        if (tree && o.reload) {
            tree.getRootNode().reload();
            tree.expandAll();
        }
    }

    function updatenode(o) {
        if ('change' in o || 'delete' in o) {
            tree.getRootNode().reload();
            tree.expandAll();
        }
    }

    function updatetitle(o) {
        var n = tree.getNodeById(o.uuid);
        if (n) {
            if (o.text)
                n.setText(o.text);
            tree.getRootNode().reload();
            tree.expandAll();
            // cannot get this to properly reload children and maintain state
        }
    }

    function builder() {
        if (tree)
            return;

        if (conf.builder)
            conf.builder(conf);

        var first = true;
        var current = null;
        var uuid = null;
        var params = conf.params || {};
        var uilevel = tvheadend.uilevel;
        var loader = new Ext.tree.TreeLoader({
            dataUrl: conf.url,
            baseParams: params,
            preloadChildren: conf.preload,
            nodeParameter: 'uuid'
        });

        var node_added = function(n) {
            var event = n.attributes.event;
            if (n.attributes.uuid && event && !(event in events)) {
                events[event] = 1;
                tvheadend.comet.on(event, updatenode);
            }
            if (n.attributes.uuid === uuid)
                n.select();
        }

        loader.on('load', function(l, n, r) {
            node_added(n);
            for (var i = 0; i < n.childNodes.length; i++)
                node_added(n.childNodes[i]);
            if (first) { /* hack */
                dpanel.doLayout();
                first = false;
            }
        });

        tree = new Ext.tree.TreePanel({
            loader: loader,
            autoWidth: true,
            flex: 1,
            autoScroll: true,
            border: false,
            animate: false,
            root: new Ext.tree.AsyncTreeNode({
                id: conf.root || 'root',
                text: conf.title || ''
            }),
            listeners: {
                click: function(n) {
                    if (current) {
                        mpanel.remove(current);
                        current = null;
                        uuid = null;
                    }
                    if (!n.isRoot) {
                        uuid = n.attributes.uuid;
                        current = new tvheadend.idnode_editor(uilevel, n.attributes, {
                            title: _('Parameters'),
                            width: 550,
                            noautoWidth: true,
                            fixedHeight: true,
                            noApply: true,
                            help: conf.help || null
                        });
                        mpanel.add(current);
                        mpanel.doLayout();
                    }
                }
            }
        });

        var mpanel = new Ext.Panel({
            layout: 'hbox',
            padding: 5,
            border: false,
            layoutConfig: {
                align: 'stretch'
            },
            items: [tree]
        });

        tree.on('beforerender', function() {
            // To be honest this isn't quite right, but it'll do
            tree.expandAll();
        });

        dpanel.add(mpanel);
        dpanel.doLayout(false, true);

        if (conf.comet)
            tvheadend.comet.on(conf.comet, update);

        tvheadend.comet.on('title', updatetitle);
    }

    function destroyer() {
        if (tree === null || !tvheadend.dynamic)
            return;
        for (var event in events)
            tvheadend.comet.un(event, updatenode);
        delete events;
        events = {};
        tvheadend.comet.un('title', updatetitle);
        if (conf.comet)
            tvheadend.comet.un(conf.comet, update);
        dpanel.removeAll(true);
        tree = null;
        if (conf.destroyer)
            conf.destroyer(conf);
    }

    var dpanel = new Ext.Panel({
        id: conf.id || null,
        border: false,
        header: false,
        layout: 'fit',
        title: conf.title || '',
        iconCls: conf.iconCls || ''
    });

    tvheadend.idnode_panel(conf, panel, dpanel, builder, destroyer);
};

/*
 * Simple Node Editor
 */
tvheadend.idnode_simple = function(panel, conf)
{
    var mpanel = null;
    var update_cb = null;

    function update() {
        if (update_cb)
            update_cb(1);
    }

    function builder() {
        if (mpanel)
            return;

        if (conf.builder)
            conf.builder(conf);

        var buttons = [];
        var abuttons = {};
        var current = null;
        var uilevel = tvheadend.uilevel;
        var lastdata = null;

        function fonchange(f, o, n) {
            if (current)
                conf.onchange(current, f, o, n);
        }

        function form_build(d) {

            var fpanel = new Ext.form.FormPanel({
                frame: true,
                border: true,
                bodyStyle: 'padding: 5px',
                labelAlign: 'left',
                labelWidth: conf.labelWidth || 300,
                autoWidth: false,
                autoHeight: false,
                width: conf.nowidth ? null : (conf.width || 730),
                defaultType: 'textfield',
                buttonAlign: 'left',
                autoScroll: true
            });

            tvheadend.idnode_editor_form(uilevel, d.props || d.params, d.meta,
                                         fpanel, { showpwd: conf.showpwd });

            if (conf.onchange) {
                var f = fpanel.getForm().items;
                for (var i = 0; i < f.items.length; i++) {
                   var it = f.items[i];
                   it.on('check', fonchange);
                   it.on('change', fonchange);
                }
            }
            lastdata = d;
            return fpanel;

        }

        function form_destroy() {
            if (current)
                mpanel.remove(current);
            current = null;
        }

        /* Top bar */
        abuttons.save = new Ext.Toolbar.Button({
            tooltip: conf.saveTooltip || _('Save pending changes (marked with red border)'),
            iconCls: 'save',
            text: conf.saveText || _('Save'),
            disabled: true,
            handler: function() {
                var node = current.getForm().getFieldValues();
                tvheadend.Ajax({
                    url: conf.url + '/save',
                    params: {
                        node: Ext.encode(node)
                    },
                    success: function() {
                        if (conf.postsave)
                            conf.postsave(node, abuttons);
                        form_load(true);
                    }
                });
            }
        });
        buttons.push(abuttons.save);
        abuttons.undo = new Ext.Toolbar.Button({
            tooltip: _('Revert pending changes (marked with red border)'),
            iconCls: 'undo',
            text: _('Undo'),
            disabled: true,
            handler: function() {
                if (current)
                    current.getForm().reset();
            }
        });
        buttons.push(abuttons.undo);

        /* Extra buttons */
        if (conf.tbar) {
            buttons.push('-');
            for (i = 0; i < conf.tbar.length; i++) {
                var t = conf.tbar[i];
                if (t.name && t.builder) {
                    var b = t.builder();
                    if (t.callback) {
                        b.callback = t.callback;
                        b.handler = function(b, e) {
                            this.callback(this, e);
                        }
                    }
                    abuttons[t.name] = b;
                    buttons.push(b);
                } else if (t.name)
                    buttons.push(t.name);
            }
        }

        function uilevel_change(l, refresh) {
            if (l === uilevel)
                return;
            uilevel = l;
            if (!refresh)
                return;
            var values = null;
            if (current)
                values = current.getForm().getFieldValues();
            form_destroy();
            if (lastdata) {
                current = form_build(lastdata);
                if (values && current)
                     current.getForm().setValues(values);
                if (current) {
                     mpanel.add(current);
                     mpanel.doLayout();
                }
            }
        }

        if (!tvheadend.uilevel_nochange && (!conf.uilevel || conf.uilevel !== 'expert')) {
            abuttons.uilevel = tvheadend.idnode_uilevel_menu(uilevel, uilevel_change);
            buttons.push('->');
            buttons.push(abuttons.uilevel);
        }

        /* Help */
        if (conf.help) {
            buttons.push(abuttons.uilevel ? '-' : '->');
            buttons.push({
                text: _('Help'),
                iconCls: 'help',
                handler: conf.help
            });
        }

        function form_load(force) {
            if (!force && current)
                return;
            var params = conf.edit ? (conf.edit.params || {}) : {};
            params.meta = 1;
            tvheadend.Ajax({
                url: conf.url + '/load',
                params: params,
                success: function(d) {
                    d = json_decode(d);
                    form_destroy();
                    current = new form_build(d[0]);
                    abuttons.save.setDisabled(false);
                    abuttons.undo.setDisabled(false);
                    mpanel.add(current);
                    mpanel.doLayout();
                }
            });
        }

        mpanel = new Ext.Panel({
            tbar: buttons,
            layout: 'hbox',
            padding: 5,
            border: false,
            layoutConfig: {
               align: 'stretch'
            }
        });
        
        dpanel.add(mpanel);
        dpanel.doLayout(false, true);

        mpanel.on('uilevel', function() {
            uilevel_change(tvheadend.uilevel, 1);
        });

        if (conf.comet) {
            update_cb = form_load;
            tvheadend.comet.on(conf.comet, update);
        }

        form_load(true);
    }

    function destroyer() {
        if (mpanel === null || !tvheadend.dynamic)
            return;
        if (conf.comet) {
            update_cb = null;
            tvheadend.comet.un(conf.comet, update);
        }
        dpanel.removeAll(true);
        mpanel.purgeListeners();
        mpanel = null;
        if (conf.destroyer)
            conf.destroyer(conf);
    }

    var dpanel = new Ext.Panel({
        id: conf.id || null,
        border: false,
        header: false,
        layout: 'fit',
        title: conf.title || '',
        iconCls: conf.iconCls || ''
    });

    dpanel.on('show', function() {
        if (mpanel)
            mpanel.fireEvent('uilevel');
    });

    tvheadend.idnode_panel(conf, panel, dpanel, builder, destroyer);
};
