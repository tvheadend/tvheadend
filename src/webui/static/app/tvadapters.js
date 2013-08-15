tvheadend.tvadapters = function() {
//  return tvheadend.item_browser('tvadapters', 'TV Adapters');
  return tvheadend.idnode_tree({ url: 'api/tvadapter/tree', title: 'TV adapters'});
}
