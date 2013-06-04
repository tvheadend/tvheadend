/*
 * IDnode stuff
 */

/*
 * DVB network
 */

tvheadend.networks = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url  : 'api/mpegts/network',
    title: 'Networks'
  });
}

tvheadend.muxes = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url  : 'api/mpegts/mux',
    title: 'Muxes'
  });
}

tvheadend.services = function(panel)
{
  tvheadend.idnode_grid(panel, {
    url  : 'api/mpegts/service',
    title: 'Services'
  });
}
