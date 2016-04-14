: 

The following placeholders are available:

Placeholder | Function
:----------:| --------
**%C**      | The transliterated channel name in ASCII (safe characters, no spaces, etc. - so `Das Erste HD` will be `Das_Erste_HD`, but `WDR Köln` will be `WDR_Koln`)
**%c**      | The channel name (URL encoded ASCII)

Example: `file:///tmp/icons/%C.png` or `http://example.com/%c.png`
