


tvheadend.item_editor = function(item) {
  var propsGrid = new Ext.grid.PropertyGrid({
    flex:1,
    padding: 5,
    propertyNames: item.propertynames,
    source: item.properties
  });
  return propsGrid;
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
