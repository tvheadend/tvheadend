## FAQ: Frequently-asked Questions

### Q: How do I get a playlist for all my channels?

Tvheadend can generate a playlist of all your mapped services (channels). You can download it from the webui at `http://IP:Port/playlist`, e.g. `http://192.168.0.2:9981/playlist`.

### Q: Why am I getting a playlist when trying to view/stream a channel?

By default Tvheadend's *Play* links are playlists, although not all players accept them (e.g. Media Player Classic Home Cinema). You can bypass this by removing the `/play/` path from the url.

### Q: Tvheadend has scanned for services but some rows in the Service Name column are blank, is that normal?

Yes, not all services are given a name by providers. These services are usually hidden for a reason and are often used for things such as encrypted guide data for set-top boxes, interactive services, and so on.

If you're not seeing any service names at all this may indicate an issue with your hardware and/or configuration.

### Q: I get a blank page when trying to view the web interface!

This usually happens when Tvheadend is installed incorrectly. As a start, make sure that the web interface path `/usr/share/tvheadend/src/webui/static/` exists and isn't empty. 

Note: The above path only applies to Debian/Ubuntu systems others may differ.

### Q: Why can't I see my tuners in Tvheadend's interface?

This is normally because they're not installed properly. Check syslog/dmesg (e.g. `dmesg | grep dvb`) and see that you have startup 
messages that indicate whether or not the tuners have initialized properly. Similarly, check `/dev/dvb` to 
see if the block device files (i.e. the files used to communicate with the tuner) have been created correctly.

The other major cause of this issue is when you're running Tvheadend as a user who doesn't have sufficient
access to the tuners, such as not being a member of the *video* group.

### Q: Access Tvheadend through HTTP proxy

Use '--http_root' directive to specify the alternative http webroot (initial
path prefix). The proxy server *MUST* pass this webroot path in the HTTP
request, otherwise an access to the Tvheadend server will end with
the endless redirect loop.

Example for nginx (--http_root /my/tvh/server):

```
	location /my/tvh/server {
		proxy_pass http://1.1.1.1:9981;
		proxy_set_header Host $host;
		proxy_set_header X-Real-IP $remote_addr;
		proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
		proxy_set_header Upgrade $http_upgrade;
		proxy_set_header Connection "upgrade";
	}
```

Example for apache (--http_root=/my/tvh/server):

```
ProxyPass "/my/tvh/server" "http://1.1.1.9981/my/tvh/server";
```
