: The string allow to manually specify the full path generation using
  the predefined modifiers for strftime (see `man strftime`, except
  `%n` and `%t`) and Tvheadend specific. Note that you may modify some of
  this format string setting using the GUI fields below.

Format    | Description                                      | Example
:--------:|--------------------------------------------------|--------
`$t$n.$x` | Default format (title, unique number, extension) | Tennis - Wimbledon-1.mkv
`$s`      | Event subtitle name                              | Sport
`$t`      | Event title name                                 | Tennis - Wimbledon
`$e`      | Event episode name                               | S02-E06
`$c`      | Channel name                                     | SkySport
`$g`      | Content type                                     | Movie : Science fiction
`$n`      | Unique number added when the file already exists | -1
`$x`      | Filename extension (from the active stream muxer | mkv
`%F`      | ISO 8601 date format                             | 2011-03-19
`%R`      | The time in 24-hour notation                     | 14:12

The format strings `$t`,`$s`,`%e`,`$c` also have delimiter variants such as 
`$ t` (space after the dollar character), `$-t`, `$_t`,
`$.t`, `$,t`, `$;t`. In these cases, the delimiter is applied 
only when the substituted string is not empty.

For $t and $s format strings, you may also limit the number of output
characters using $99-t format string where 99 means the limit. As you can
see, the delimiter can be also applied.
