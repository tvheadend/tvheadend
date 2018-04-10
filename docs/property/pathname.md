: The string allows you to manually specify the full path generation using
  the predefined modifiers for strftime (see `man strftime`, except
  `%n` and `%t`) and Tvheadend specific. Note that you may modify some of
  this format string setting using the GUI fields below.

Format    | Description                                      | Example
:--------:|--------------------------------------------------|--------
`$t$n.$x` | Default format (title, unique number, extension) | Tennis - Wimbledon-1.mkv
`$t`      | Event title name                                 | Tennis - Wimbledon
`$s`      | Event subtitle name or summary text              | Live Tennis Broadcast from Wimbledon
`$u`      | Event subtitle name                              | Tennis
`$m`      | Event summary text                               | Live Tennis Broadcast from Wimbledon
`$e`      | Event episode name                               | S02-E06
`$c`      | Channel name                                     | SkySport
`$g`      | Content type                                     | Movie : Science fiction
`$Q`      | Scraper friendly (see below)                     | Gladiator (2000)
 〃       | 〃                                               | Bones - S02E06
`$q`      | Scraper friendly with directories (see below)    | tvshows/Bones/Bones - S02E06
 〃       | 〃                                               | tvmovies/Gladiator (2000)
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

The format strings `$q` and `$Q` generate filenames that are suitable
for many external scrapers. They rely on correct schedule data that correctly
identifies episodes and genres. If your guide data incorrectly
identifies movies as shows then the filenames will be incorrect and
show could be identifies as movies or vice-versa. Any xmltv guide data
should contain the category "movie" for movies.

The `$q` format will create sub-directories `tvmovies` and `tvshows`
based on the genre in the guide data. For tvshows a second-level
directory based on the title of the show is created.

Examples are:
- tvmovies/Gladiator (2000)
- tvshows/Countdown/Countdown
- tvshows/Bones/Bones - S05E11
- tvshows/Bones/Bones - S05E11 - The X in the Files

The `$Q` format is similar to `$q` but does not use genre sub-directories.
Sub-directories are still created for tvshow episodes.
Examples are below based on different information in the EPG:
- Gladiator (2000) (movie)
- Bones/Bones - S05 E11 (episode with guide season/episode information)
- Countdown/Countdown (episode without guide season/episode information)

The `$Q` and `$q` formats also have two numeric modifiers to select
variant formats and can be used as `$1Q`, `$2Q`, `$1q`, and `$2q`.

The number 1 variant forces the recording to be formatted as a movie,
ignoring the genre from the schedule.

Whereas the number 2 variant forces the recording to be formatted as a
tv series.

These variants can be useful to work-around bad schedule data that gives
incorrect genres for programmes.

Typically the `$q` and `$Q` formats would be combined with other
modifiers to generate a complete filename such as `$q$n.$x`.

Even with correct guide information, external scrapers can retrieve
incorrect results. A famous example being the detective tv series
"Castle" is often incorrectly retrieved as a much earlier tv show
about castles.
