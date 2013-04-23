
tvheadend.item_editor = function(item) {

  var fields = []

  for (var idx in item.params) {
    var f = item.params[idx];
    switch(f.type) {
    case 'str':
      fields.push({
        fieldLabel: f.caption,
        name: f.id,
        value: f.value
      });
      break;

    case 'bool':
      fields.push({
        xtype: 'checkbox',
        fieldLabel: f.caption,
        name: f.id,
        checked: f.value
      });
      break;

    case 'separator':
      fields.push({
        xtype: 'label',
        fieldLabel: f.caption,
      });
      break;
    }
  }

  var panel = new Ext.FormPanel({
    labelWidth: 150, // label settings here cascade unless overridden
    frame:true,
    title: 'Parameters',
    bodyStyle:'padding:5px 5px 0',
    width: 500,
    defaults: {width: 330},
    defaultType: 'textfield',
    items: fields,

    buttons: [{
      text: 'Save',
      handler: function(){
        if(panel.getForm().isValid()){
          panel.getForm().submit({
            url: 'item/update/' + item.id,
	    waitMsg : 'Saving Data...'
          });
        }
      }
    },{
      text: 'Reset',
      handler: function(){
        panel.getForm().reset();
      }
    }]
  });
  return panel;
}







/**
 *
 */
tvheadend.item_browser = function(url, title) {

  var current = null;

  var loader = new Ext.tree.TreeLoader({
    dataUrl: url
  });

  var tree = new Ext.tree.TreePanel({
    loader: loader,
    flex:1,
    border: false,
    root : new Ext.tree.AsyncTreeNode({
      id : 'root',
      text: title
    }),
    listeners: {
      click: function(n) {
        if(current)
          panel.remove(current);
        current = panel.add(new tvheadend.item_editor(n.attributes));
        panel.doLayout();
      }
    }
  });

  tvheadend.comet.on('idnodeNameChanged', function(o) {
    var n = tree.getNodeById(o.id);
    if(n) {
      n.setText(o.text);
    }
  });

  tvheadend.comet.on('idnodeParamsChanged', function(o) {
    var n = tree.getNodeById(o.id);
    if(n) {
      n.attributes.params = o.params;
   }
  });


  var panel = new Ext.Panel({
    title: title,
    layout: 'hbox',
    flex: 1,
    padding: 5,
    border: false,
    layoutConfig: {
      align:'stretch'
    },
    items: [tree]
  });


  tree.on('render', function() {
    tree.getRootNode().expand();
  });

  return panel;
}


/**
 *
 */
tvheadend.dvb_networks = function() {
  return tvheadend.item_browser('/dvb/networks', 'DVB Networks');
}

