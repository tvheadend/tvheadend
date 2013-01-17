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
    })
  });


  tree.on('render', function() {
    tree.getRootNode().expand();
  });

  return tree;
}
