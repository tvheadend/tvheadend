: Command to run when a recording starts. The command will be run in
  background.

 Supported format strings:


Format | Description                               | Example value
:-----:| ----------------------------------------- | -------------
`%f`   | Full path to recording                    |  /home/user/Videos/News.mkv
`%b`   | Basename of recording                     |  News.mkv
`%c`   | Channel name                              |  BBC world
`%O`   | Owner of this recording                   |  user
`%C`   | Who created this recording                |  user
`%t`   | Program title                             |  News
`%s`   | Program subtitle                          |  Afternoon
`%p`   | Program episode                           |  S02.E07
`%d`   | Program description                       |  News and stories…
`%S`   | Start time stamp of recording, UNIX epoch |  1224421200
`%E`   | Stop time stamp of recording, UNIX epoch  |  1224426600
`%Z`   | Comment                                   |  A string

*Example usage*

To use special characters (e.g. spaces), either put the string in quotes or
escape the individual characters.

```/usr/bin/lcd_show "%f"```
