/**
 *
 */
tvheadend.dvb_networks = function() {

  var loader = new Ext.tree.TreeLoader({
    dataUrl: 'dvb/networks'
  });

  var tree = new Ext.tree.TreePanel({
    title: 'DVB Networks',
    loader: loader,
    root : new Ext.tree.AsyncTreeNode({
      id : 'root',
      text: 'DVB Networks'
    }),
    listeners: {
      click: function(n) {
        Ext.Msg.alert('Navigation Tree Click', 'You clicked: "' + n.attributes.text + '"');
      }
    }
  });


  tree.on('render', function() {
    tree.getRootNode().expand();
  });

  return tree;
}
