This code may be used instead of/along side the password to access playlists/streams.

Method                                     | Example
-------------------------------------------|------------------------------------
HTTP authentication (digest/plain)         | ```http://ip:9981/playlist/auth``` or ```http://user:pass@ip:9981/playlist/auth```
Authcode only                              | ```http://ip:9981/playlist/auth?auth={authcode}```
Channel 'play' streams                     | ```http://ip:9981/play/stream/channelname/CHANNELNAME?auth={authcode}```
