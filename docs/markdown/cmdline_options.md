## Command-line Options

Usage: `tvheadend [OPTIONS]`

### Generic options

```
  -h, --help                  Show this page
  -v, --version               Show version information
```
### Service configuration
```
  -c, --config                Alternate configuration path
  -B, --nobackup              Don't backup configuration tree at upgrade
  -f, --fork                  Fork and run as daemon
  -u, --user                  Run as user
  -g, --group                 Run as group
  -p, --pid                   Alternate PID path
  -C, --firstrun              If no user account exists then create one with
                              no username and no password. Use with care as
                              it will allow world-wide administrative access
                              to your Tvheadend installation until you create or edit
                              the access control from within the Tvheadend web interface.
  -U, --dbus                  Enable DBus
  -e, --dbus_session          DBus - use the session message bus instead of the system one
  -a, --adapters              Only use specified DVB adapters (comma-separated, -1 = none)
      --satip_bindaddr        Specify bind address for SAT>IP server
      --satip_rtsp            SAT>IP RTSP port number for server
                              (default: -1 = disable, 0 = webconfig, standard port is 554)
      --nosatipcli            Disable SAT>IP client
      --satip_xml             URL with the SAT>IP server XML location
```
### Server connectivity
```
  -6, --ipv6                  Listen on IPv6
  -b, --bindaddr              Specify bind address
      --http_port             Specify alternative http port
      --http_root             Specify alternative http webroot
      --htsp_port             Specify alternative htsp port
      --htsp_port2            Specify extra htsp port
      --useragent             Specify User-Agent header for the http client
      --xspf                  Use XSPF playlist instead of M3U
```
### Debug options
```
  -d, --stderr                Enable debug on stderr
  -n, --nostderr              Disable debug on stderr
  -s, --syslog                Enable debug to syslog
  -S, --nosyslog              Disable syslog (all messages)
  -l, --logfile               Enable debug to file
      --debug                 Enable debug subsystems
      --trace                 Enable trace subsystems
      --subsystems            List subsystems
      --fileline              Add file and line numbers to debug
      --threadid              Add the thread ID to debug
      --uidebug               Enable web UI debug (non-minified JS)
  -A, --abort                 Immediately abort
  -D, --dump                  Enable coredumps for daemon
      --noacl                 Disable all access control checks
      --nobat                 Disable DVB bouquets
  -j, --join                  Subscribe to a service permanently
```
### Testing options
```
      --tsfile_tuners         Number of tsfile tuners
      --tsfile                tsfile input (mux file)
```
