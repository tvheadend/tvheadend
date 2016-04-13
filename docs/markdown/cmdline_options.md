##Command-line Options

Usage: `tvheadend [OPTIONS]`

###Generic Options

```no-highlight
-h, --help                  Show this page
-v, --version               Show version infomation
```

###Service Configuration

```no-highlight
-c, --config                Alternate config path
-B, --nobackup              Do not backup config tree at upgrade
-f, --fork                  Fork and run as daemon
-u, --user                  Run as user
-g, --group                 Run as group
-p, --pid                   Alternate pid path
-C, --firstrun              If no user account exists then create one with
                            no username and no password. Use with care as
                            it will allow world-wide administrative access
                            to your Tvheadend installation until you edit
                            the access-control from within the Tvheadend UI
-U, --dbus                  Enable DBus
-e, --dbus_session          DBus - use the session message bus instead system one
-a, --adapters              Only use specified DVB adapters (comma separated)
    --satip_rtsp            SAT>IP RTSP port number for server
                            (default: -1 = disable, 0 = webconfig, standard port is 554)
    --satip_xml             URL with the SAT>IP server XML location
```

###Server Connectivity

```no-highlight
-6, --ipv6                  Listen on IPv6
-b, --bindaddr              Specify bind address
    --http_port             Specify alternative http port
    --http_root             Specify alternative http webroot
    --htsp_port             Specify alternative htsp port
    --htsp_port2            Specify extra htsp port
    --useragent             Specify User-Agent header for the http client
    --xspf                  Use xspf playlist instead M3U
```

###Debug Options

```ini
-d, --stderr                Enable debug on stderr
-n, --nostderr              Disable debug on stderr
-s, --syslog                Enable debug to syslog
-S, --nosyslog              Disable syslog (all msgs)
-l, --logfile               Enable debug to file
    --debug                 Enable debug subsystems
    --trace                 Enable trace subsystems
    --fileline              Add file and line numbers to debug
    --threadid              Add the thread ID to debug
    --libav                 More verbose libav log
    --uidebug               Enable webUI debug (non-minified JS)
-A, --abort                 Immediately abort
-D, --dump                  Enable coredumps for daemon
    --noacl                 Disable all access control checks
    --nobat                 Disable DVB bouquets
-j, --join                  Subscribe to a service permanently
```

###Testing Options

```no-highlight
--tsfile_tuners         Number of tsfile tuners
--tsfile                tsfile input (mux file)
```
