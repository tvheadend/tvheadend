
tvheadend.item_editor = function(item) {

  var fields = []

  for (var idx in item.descriptors) {
    var f = item.descriptors[idx];
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
    labelWidth: 75, // label settings here cascade unless overridden
    url:'save-form.php',
    frame:true,
    title: 'Parameters',
    bodyStyle:'padding:5px 5px 0',
    width: 350,
    defaults: {width: 230},
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
tvheadend.dvb_networks = function() {

  var current = null;

  var loader = new Ext.tree.TreeLoader({
    dataUrl: 'dvb/networks'
  });

  var tree = new Ext.tree.TreePanel({
    loader: loader,
    flex:1,
    border: false,
    root : new Ext.tree.AsyncTreeNode({
      id : 'root',
      text: 'DVB Networks'
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

  tvheadend.comet.on('idnodeDescriptorsChanged', function(o) {
    var n = tree.getNodeById(o.id);
    if(n) {
      n.attributes.descriptors = o.descriptors;
   }
  });


  var panel = new Ext.Panel({
    title: 'DVB Networks',
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
