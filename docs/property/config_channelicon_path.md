: 

The following placeholders are available:

Placeholder | Function
:----------:| --------
**%C**      | The transliterated channel name in URL encoded ASCII with safe characters only - `WDR Köln :<>*?'"` will be `WDR%20Koln%20________`
**%c**      | The transliterated channel name in URL encoded ASCII
**%U**      | UTF-8 encoded URL

Example: `file:///tmp/icons/%C.png` or `http://example.com/%c.png`

Note: The `file://` URLs are deescaped back when used, so `%20` means space
for the filename for example.

Safety note: For the channel name, the first dot characters (possible
hidden files or special directories) are replaced with the underscore
character. The possible directory delimiters (slash) and the special
character backslash are replaced with the minus character.
