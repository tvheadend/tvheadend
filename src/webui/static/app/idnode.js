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

    /* Build store */
    var st = new Ext.data.JsonStore({
        root: conf.root || 'entries',
        url: conf.url,
        baseParams: conf.params || {},
        fields: conf.fields || ['key', 'val'],
        id: conf.id || 'key',
        autoLoad: true,
        listeners: conf.listeners || {},
        sortInfo: conf.sort || {
            field: 'val',
            direction: 'ASC'
        }
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
    if (f.enum.type === 'api') {
        return tvheadend.idnode_get_enum({
            url: 'api/' + f.enum.uri,
            params: f.enum.params,
            event: f.enum.event
        });
    }

    switch (f.type) {
        case 'str':
            if (f.enum.length > 0 && f.enum[0] instanceof Object)
                store = new Ext.data.JsonStore({
                    id: 'key',
                    fields: ['key', 'val'],
                    data: f.enum
                });
            else
                store = f.enum;
            break;
        case 'int':
        case 'u32':
        case 'u16':
        case 's64':
        case 'dbl':
        case 'time':
            var data = null;
            if (f.enum.length > 0 && f.enum[0] instanceof Object) {
                data = f.enum;
            } else {
                data = [];
                for (i = 0; i < f.enum.length; i++)
                    data.push({key: i, val: f.enum[i]});
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

tvheadend.IdNodeField = function(conf)
{
    /*
     * Properties
     */
    this.id = conf.id;
    this.text = conf.caption || this.id;
    this.type = conf.type;
    this.list = conf.list;
    this.rdonly = conf.rdonly;
    this.wronly = conf.wronly;
    this.wronce = conf.wronce;
    this.hidden = conf.hidden || conf.advanced;
    this.password = conf.password;
    this.duration = conf.duration;
    this.group = conf.group;
    this.enum = conf.enum;
    this.store = null;
    if (this.enum)
        this.store = tvheadend.idnode_enum_store(this);
    this.ordered = false;

    /*
     * Methods
     */

    this.column = function(conf)
    {
        var w = 300;
        var ftype = 'string';
        if (this.type === 'int' || this.type === 'u32' ||
            this.type === 'u16' || this.type === 's64' ||
            this.type === 'dbl') {
            ftype = 'numeric';
            w = 80;
        } else if (this.type === 'time') {
            w = 120;
            if (this.durations) {
              ftype = 'numeric';
              w = 80;
            }
        } else if (this.type === 'bool') {
            ftype = 'boolean';
            w = 60;
        }
        if (this.enum || this.list)
            w = 300;

        if (conf && this.id in conf) {
          if (conf[this.id].width)
            w = conf[this.id].width;
        }

        var props = {
            width: w,
            dataIndex: this.id,
            header: this.text,
            editor: this.editor({create: false}),
            renderer: this.renderer(),
            editable: !this.rdonly,
            hidden: this.hidden,
            filter: {
                type: ftype,
                dataIndex: this.id
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

    this.renderer = function()
    {
        if (this.password)
            return function(v) {
                return '<span class="tvh-grid-unset">********</span>';
            }
            
        if (this.type === 'time') {
            if (this.duration)
                return function(v) {
                    v = parseInt(v / 60); /* Nevermind the seconds */
                    if (v === 0 && v !== '0')
                       return "Not set";
                    var hours = parseInt(v / 60);
                    var min = parseInt(v % 60);
                    if (hours) {
                        if (min === 0)
                            return hours + ' hrs';
                        return hours + ' hrs, ' + min + ' min';
                    }
                    return min + ' min';
                }
            return function(v) {
                var dt = new Date(v * 1000);
                return dt.format('D j M H:i');
            }
        }

        if (!this.store)
            return null;

        var st = this.store;
        return function(v) {
            if (st && st instanceof Ext.data.JsonStore) {
                var t = [];
                var d = v.push ? v : [v];
                for (var i = 0; i < d.length; i++) {
                    var r = st.find('key', d[i]);
                    if (r !== -1) {
                        var nv = st.getAt(r).get('val');
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
        if (this.enum) {
            cons = Ext.form.ComboBox;
            if (this.list)
                cons = Ext.ux.form.LovCombo;

            /* Combo settings */
            c['mode'] = 'local';
            c['valueField'] = 'key';
            c['displayField'] = 'val';
            c['store'] = this.store;
            c['typeAhead'] = true;
            c['forceSelection'] = false;
            c['triggerAction'] = 'all';
            c['emptyText'] = 'Select ' + this.text + ' ...';

            /* Single */
        } else {

            if (this.type == 'perm') {
                c['regex'] = /^[0][0-7]{3}$/;
                c['maskRe'] = /[0-7]/;
                c['allowBlank'] = false;
                c['blankText'] = 'You must provide a value - use octal chmod notation, e.g. 0664';
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
                case 'dbl':
                case 'time':
                    cons = Ext.form.NumberField;
                    break;

                /* 'str' and 'perm' */
                default:
                    cons = Ext.form.TextField;
                    break;
            }
        }

        return new cons(c);
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
    this.clazz = conf.class;
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
 * Field editor
 */
tvheadend.idnode_editor_field = function(f, create)
{
    var d = f.rdonly || false;
    if (f.wronly && !create)
        d = false;
    var value = f.value;
    if (value == null)
        value = f.default;

    /* Enumerated (combobox) type */
    if (f.enum) {
        var cons = Ext.form.ComboBox;
        if (f.list)
            cons = Ext.ux.form.LovCombo;
        var st = tvheadend.idnode_enum_store(f);
        var r = new cons({
            fieldLabel: f.caption,
            name: f.id,
            value: value,
            disabled: d,
            width: 300,
            mode: 'local',
            valueField: 'key',
            displayField: 'val',
            store: st,
            typeAhead: true, // TODO: this does strange things in multi
            forceSelection: false,
            triggerAction: 'all',
            emptyText: 'Select ' + f.caption + ' ...',
            listeners: {
                beforequery: function(qe) {
                    delete qe.combo.lastQuery;
                }
            }
        });
        if (st.on) {
            var fn = function() {
                st.un('load', fn);
                r.setValue(value); // HACK: to get extjs to display properly
            };
            st.on('load', fn);
        }
        return r;
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

    /* Singular */
    switch (f.type) {
        case 'bool':
            return new Ext.ux.form.XCheckbox({
                fieldLabel: f.caption,
                name: f.id,
                checked: value,
                disabled: d
            });

        case 'time':
            if (!f.duration)
                return new Ext.ux.form.TwinDateTimeField({
                    fieldLabel: f.caption,
                    name: f.id,
                    value: value,
                    disabled: d,
                    width: 300,
                    timeFormat: 'H:i:s',
                    timeConfig: {
                        altFormats: 'H:i:s',
                        allowBlank: true,
                        increment: 10,
                    },
                    dateFormat:'d.n.Y',
                    dateConfig: {
                        altFormats: 'Y-m-d|Y-n-d',
                        allowBlank: true,
                    }
                });
            /* fall thru!!! */

        case 'int':
        case 'u32':
        case 'u16':
        case 's64':
        case 'dbl':
            return new Ext.form.NumberField({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                width: 300
            });

        case 'perm':
            return new Ext.form.TextField({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                width: 125,
                regex: /^[0][0-7]{3}$/,
                maskRe: /[0-7]/,
                allowBlank: false,
                blankText: 'You must provide a value - use octal chmod notation, e.g. 0664'
            });


        default:
            return new Ext.form.TextField({
                fieldLabel: f.caption,
                name: f.id,
                value: value,
                disabled: d,
                width: 300
            });

    }
};

/*
 * ID node editor form fields
 */
tvheadend.idnode_editor_form = function(d, meta, panel, create)
{
    var af = [];
    var rf = [];
    var df = [];
    var groups = null;

    /* Fields */
    for (var i = 0; i < d.length; i++) {
        var p = d[i];
        var f = tvheadend.idnode_editor_field(p, create);
        if (!f)
            continue;
        if (p.group && meta.groups) {
            if (!groups)
                groups = {};
            if (!(p.group in groups))
                groups[p.group] = [f];
            else
                groups[p.group].push(f);
        } else {
            if (p.rdonly)
                rf.push(f);
            else if (p.advanced)
                af.push(f);
            else
                df.push(f);
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
                       collapsed: false,
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
                var p = newFieldSet({ title: m.name || "Settings", layout: 'column', border: false });
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
            for (var i = 0; i < g.length; i++)
                cfs[number].add(g[i]);
            if (number in mfs)
                panel.add(mfs[number]);
        }
    }
    if (df.length)
        panel.add(newFieldSet({ title: "Basic Settings", items: df }));
    if (af.length)
        panel.add(newFieldSet({ title: "Advanced Settings", items: af }));
    if (rf.length)
        panel.add(newFieldSet({ title: "Read-only Info", items: rf }));
    panel.doLayout();
};

/*
 * ID node editor panel
 */
tvheadend.idnode_editor = function(item, conf)
{
    var panel = null;
    var buttons = [];

    /* Buttons */
    if (!conf.noButtons) {
        var saveBtn = new Ext.Button({
            text: 'Save',
            handler: function() {
                var node = panel.getForm().getFieldValues();
                node.uuid = item.uuid;
                tvheadend.Ajax({
                    url: 'api/idnode/save',
                    params: {
                        node: Ext.encode(node)
                    },
                    success: function(d) {
                        if (conf.win)
                            conf.win.hide();
                    }
                });
            }
        });
        buttons.push(saveBtn);

        if (conf.help) {
            var helpBtn = new Ext.Button({
                text: 'Help',
                handler: conf.help
            });
            buttons.push(helpBtn);
        }
    }

    panel = new Ext.form.FormPanel({
        title: conf.title || null,
        frame: true,
        border: conf.inTabPanel ? false : true,
        bodyStyle: 'padding: 5px',
        labelAlign: 'left',
        labelWidth: conf.labelWidth || 200,
        autoWidth: conf.noautoWidth ? false : true,
        autoHeight: !conf.fixedHeight,
        width: conf.width || 600,
        //defaults: {width: 330},
        defaultType: 'textfield',
        buttonAlign: 'left',
        autoScroll: true,
        buttons: buttons,
    });

    tvheadend.idnode_editor_form(item.props || item.params, item.meta, panel, false);

    return panel;
};


/*
 * IDnode creation dialog
 */
tvheadend.idnode_create = function(conf, onlyDefault)
{
    var puuid = null;
    var panel = null;
    var pclass = null;

    /* Buttons */
    var saveBtn = new Ext.Button({
        tooltip: 'Create new entry',
        text: 'Create',
        hidden: true,
        handler: function() {
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
        }
    });
    var undoBtn = new Ext.Button({
        tooltip: 'Cancel operation',
        text: 'Cancel',
        handler: function() {
            win.close();
        }
    });

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
        buttons: [undoBtn, saveBtn]
    });

    /* Create window */
    win = new Ext.Window({
        title: 'Add ' + conf.titleS,
        layout: 'fit',
        autoWidth: true,
        autoHeight: true,
        plain: true,
        items: panel
    });


    /* Do we need to first select a class? */
    if (conf.select) {
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
        if (conf.select.propField) {
            select = function(s, n, o) {
                var r = store.getAt(s.selectedIndex);
                if (r) {
                    var d = r.get(conf.select.propField);
                    if (d) {
                        pclass = r.get(conf.select.valueField);
                        win.setTitle('Add ' + s.lastSelectionText);
                        panel.remove(s);
                        tvheadend.idnode_editor_form(d, null, panel, true);
                        saveBtn.setVisible(true);
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
                        tvheadend.idnode_editor_form(d.props, d, panel, true);
                        saveBtn.setVisible(true);
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
    } else {
        tvheadend.Ajax({
            url: conf.url + '/class',
            params: conf.params,
            success: function(d) {
                d = json_decode(d);
                tvheadend.idnode_editor_form(d.props, d, panel, true);
                saveBtn.setVisible(true);
                if (onlyDefault) {
                    saveBtn.handler();
                    panel.destroy();
                } else
                    win.show();
            }
        });
    }
};


/*
 * IDnode grid
 */
tvheadend.idnode_grid = function(panel, conf)
{
    var store = null;
    var grid = null;
    var event = null;
    var auto = null;

    var update = function(o) {
        if (auto.getValue())
            store.reload();
    };

    function build(d)
    {
        if (conf.builder)
            conf.builder(conf);

        var columns = [];
        var filters = [];
        var fields = [];
        var buttons = [];
        var abuttons = {};
        var plugins = conf.plugins || [];

        /* Some copies */
        if (conf.add && !conf.add.titleS && conf.titleS)
            conf.add.titleS = conf.titleS;

        /* Left-hand columns (do copy, no reference!) */
        if (conf.lcol)
            for (i = 0; i < conf.lcol.length; i++)
                columns.push(conf.lcol[i]);

        /* Model */
        var idnode = new tvheadend.IdNode(d);
        for (var i = 0; i < idnode.length(); i++) {
            var f = idnode.field(i);
            var c = f.column(conf.columns);
            fields.push(f.id);
            columns.push(c);
            if (c.filter)
                filters.push(c.filter);
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
        store = new Ext.data.JsonStore({
            root: 'entries',
            url: conf.gridURL || (conf.url + '/grid'),
            autoLoad: true,
            id: 'uuid',
            totalProperty: 'total',
            fields: fields,
            remoteSort: true,
            pruneModifiedRecords: true,
            sortInfo: conf.sort ? conf.sort : null,
        });

        /* Model */
        var sortable = true;
        if (conf.move)
            sortable = false;
        var model = new Ext.grid.ColumnModel({
            defaultSortable: sortable,
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
                abuttons.edit.setDisabled(count !== 1);
            if (conf.selected)
                conf.selected(s, abuttons);
        });

        /* Top bar */
        if (!conf.readonly) {
            abuttons.save = new Ext.Toolbar.Button({
                tooltip: 'Save pending changes (marked with red border)',
                iconCls: 'save',
                text: 'Save',
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
                tooltip: 'Revert pending changes (marked with red border)',
                iconCls: 'undo',
                text: 'Undo',
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
                tooltip: 'Add a new entry',
                iconCls: 'add',
                text: 'Add',
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
                tooltip: 'Delete selected entries',
                iconCls: 'remove',
                text: 'Delete',
                disabled: true,
                handler: function() {
                    var r = select.getSelections();
                    if (r && r.length > 0) {
                        var uuids = [];
                        for (var i = 0; i < r.length; i++)
                            uuids.push(r[i].id);
                        tvheadend.Ajax({
                            url: 'api/idnode/delete',
                            params: {
                                uuid: Ext.encode(uuids)
                            },
                            success: function(d)
                            {
                                if (!auto.getValue())
                                    store.reload();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.del);
        }
        if (conf.move) {
            abuttons.up = new Ext.Toolbar.Button({
                tooltip: 'Move selected entries up',
                iconCls: 'moveup',
                text: 'Move Up',
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
                tooltip: 'Move selected entries down',
                iconCls: 'movedown',
                text: 'Move Down',
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
            abuttons.edit = new Ext.Toolbar.Button({
                tooltip: 'Edit selected entry',
                iconCls: 'edit',
                text: 'Edit',
                disabled: true,
                handler: function() {
                    var r = select.getSelected();
                    if (r) {
                        if (conf.edittree) {
                            var p = tvheadend.idnode_tree({
                                url: 'api/idnode/tree',
                                params: {
                                    root: r.id
                                }
                            });
                            p.setSize(800, 600);
                            var w = new Ext.Window({
                                title: 'Edit ' + conf.titleS,
                                layout: 'fit',
                                autoWidth: true,
                                autoHeight: true,
                                plain: true,
                                items: p
                            });
                            w.show();
                        } else {
                            var params = {};
                            if (conf.edit && conf.edit.params)
                              params = conf.edit.params;
                            params['uuid'] = r.id;
                            params['meta'] = 1;
                            tvheadend.Ajax({
                                url: 'api/idnode/load',
                                params: params,
                                success: function(d) {
                                    d = json_decode(d);
                                    var w = null;
                                    var c = {win: w};
                                    var p = tvheadend.idnode_editor(d[0], c);
                                    w = new Ext.Window({
                                        title: 'Edit ' + conf.titleS,
                                        layout: 'fit',
                                        autoWidth: true,
                                        autoHeight: true,
                                        plain: true,
                                        items: p
                                    });
                                    c.win = w;
                                    w.show();
                                }
                            });
                        }
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
                    data: [['default', 'Parent disabled'],
                        ['all', 'All'],
                        ['none', 'None']]
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
            buttons.push('Hide:');
            buttons.push(hidemode);
        }
        var page = new Ext.PagingToolbar({
            store: store,
            pageSize: 50,
            displayInfo: true,
            displayMsg: conf.titleP + ' {0} - {1} of {2}',
            width: 50
        });

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
                }
            }
        }

        /* Help */
        if (conf.help) {
            buttons.push('->');
            buttons.push({
                text: 'Help',
                handler: conf.help
            });
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
                    [200, '200'], [9999999999, 'All']]
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
        var page = new Ext.PagingToolbar({
            store: store,
            pageSize: 50,
            displayInfo: true,
            displayMsg: conf.titleP + ' {0} - {1} of {2}',
            emptyMsg: 'No ' + conf.titleP.toLowerCase() + ' to display',
            items: ['-', 'Auto-refresh', auto,
                '->', '-', 'Per page', count]
        });
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
        dpanel.removeAll(true);
        store.destroy();
        grid = null;
        store = null;
        auto = null;
        event = null;
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

    tvheadend.paneladd(panel, dpanel, conf.tabIndex);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
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

        var buttons = [];
        var abuttons = {};
        var plugins = conf.plugins || [];

        /* Store */
        store = new Ext.data.JsonStore({
            root: 'entries',
            url: 'api/idnode/load',
            baseParams: {
                enum: 1,
                'class': conf.clazz
            },
            autoLoad: true,
            id: 'key',
            totalProperty: 'total',
            fields: ['key','val'],
            remoteSort: false,
            pruneModifiedRecords: true,
            sortInfo: {
                field: 'val',
                direction: 'ASC'
            }
        });

        store.on('load', function(st) {
            if (!current)
                grid.getSelectionModel().selectFirstRow();
        });

        /* Model */
        var model = new Ext.grid.ColumnModel({
            defaultSortable: true,
            columns: [{
                width: 300,
                id: 'val',
                header: conf.titleC,
                sortable: true,
                dataIndex: 'val'
            }]
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
            tooltip: 'Save pending changes (marked with red border)',
            iconCls: 'save',
            text: 'Save',
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
                        store.reload()
                    }
                });
            }
        });
        buttons.push(abuttons.save);
        abuttons.undo = new Ext.Toolbar.Button({
            tooltip: 'Revert pending changes (marked with red border)',
            iconCls: 'undo',
            text: 'Undo',
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
                tooltip: 'Add a new entry',
                iconCls: 'add',
                text: 'Add',
                disabled: false,
                handler: function() {
                    tvheadend.idnode_create(conf.add, true);
                }
            });
            buttons.push(abuttons.add);
        }
        if (conf.del) {
            abuttons.del = new Ext.Toolbar.Button({
                tooltip: 'Delete selected entries',
                iconCls: 'remove',
                text: 'Delete',
                disabled: true,
                handler: function() {
                    if (current) {
                        tvheadend.Ajax({
                            url: 'api/idnode/delete',
                            params: {
                                uuid: current.uuid
                            },
                            success: function(d)
                            {
                                store.reload();
                                grid.getSelectionModel().selectFirstRow();
                            }
                        });
                    }
                }
            });
            buttons.push(abuttons.del);
        }
        if (conf.add || conf.del)
            buttons.push('-');

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
                }
            }
        }

        /* Help */
        if (conf.help) {
            buttons.push('->');
            buttons.push({
                text: 'Help',
                handler: conf.help
            });
        }

        function roweditor(r) {
            if (!r || !r.id)
                return;
            tvheadend.Ajax({
                url: 'api/idnode/load',
                params: {
                    uuid: r.id,
                    meta: 1
                },
                success: function(d) {
                    d = json_decode(d);
                    if (current)
                        mpanel.remove(current.editor);
                    var editor = new tvheadend.idnode_editor(d[0], {
                                    title: 'Parameters',
                                    labelWidth: 300,
                                    fixedHeight: true,
                                    help: conf.help || null,
                                    inTabPanel: true,
                                    noButtons: true,
                                    noautoWidth: true,
                                    width: 730
                                });
                    current = {
                        uuid: d[0].id,
                        editor: editor
                    }
                    abuttons.save.setDisabled(false);
                    abuttons.undo.setDisabled(false);
                    abuttons.del.setDisabled(false);
                    mpanel.add(editor);
                    mpanel.doLayout();
                }
            });
        }

        /* Grid Panel (Selector) */
        var grid = new Ext.grid.GridPanel({
            width: 200,
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
                            grid.getSelectionModel().selectFirstRow();
                    }
                }
            }
        });

        var mpanel = new Ext.Panel({
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
        dpanel.removeAll(true);
        store.destroy();
        mpanel = null;
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

    tvheadend.paneladd(panel, dpanel, conf.tabIndex);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
};

/*
 * IDNode Tree
 */
tvheadend.idnode_tree = function(panel, conf)
{
    var tree = null;
    var events = {};

    function update(o) {
        if (tree && o.reload)
            tree.getRootNode().reload();
    }

    function updatenode(o) {
        if (o.uuid)
            tree.getRootNode().reload();
    }

    function updatetitle(o) {
        var n = tree.getNodeById(o.uuid);
        if (n) {
            if (o.text)
                n.setText(o.text);
            tree.getRootNode().reload();
            // cannot get this to properly reload children and maintain state
        }
    }

    function builder() {
        if (tree)
            return;
        if (conf.builder)
            conf.builder(conf);

        var current = null;
        var first = true;
        var params = conf.params || {};
        var loader = new Ext.tree.TreeLoader({
            dataUrl: conf.url,
            baseParams: params,
            preloadChildren: conf.preload,
            nodeParameter: 'uuid'
        });

        loader.on('load', function(l, n, r) {
            var event = n.attributes.event;
            if (n.attributes.uuid && event && !(event in events)) {
                events[event] = 1;
                tvheadend.comet.on(event, updatenode);
            }
            if (first) { /* hack */
                dpanel.doLayout();
                first = false;
            }
        });

        tree = new Ext.tree.TreePanel({
            loader: loader,
            width: 550,
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
                    }
                    if (!n.isRoot)
                        current = mpanel.add(new tvheadend.idnode_editor(n.attributes, {
                            title: 'Parameters',
                            width: 550,
                            noautoWidth: true,
                            fixedHeight: true,
                            help: conf.help || null
                        }));
                    mpanel.doLayout();
                }
            }
        });

        var mpanel = new Ext.Panel({
            layout: 'hbox',
            flex: 1,
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
        border: false,
        header: false,
        layout: 'fit',
        title: conf.title || '',
    });

    tvheadend.paneladd(panel, dpanel, conf.tabIndex);
    tvheadend.panelreg(panel, dpanel, builder, destroyer);
};
