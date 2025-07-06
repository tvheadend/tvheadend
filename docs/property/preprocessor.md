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
`%s`   | Program subtitle or summary               |  Afternoon fast news
`%u`   | Program subtitle                          |  Afternoon
`%m`   | Program summary                           |  Afternoon fast news
`%p`   | Program episode                           |  S02.E07
`%A`   | Program season number                     |  2
`%B`   | Program episode number                    |  7
`%d`   | Program description                       |  News and stories…
`%S`   | Start time stamp of recording, UNIX epoch |  1224421200
`%E`   | Stop time stamp of recording, UNIX epoch  |  1224426600
`%U`   | Unique ID of recording                    |  3cf44328eda87a428ba9a8b14876ab80
`%Z`   | Comment                                   |  A string

*Example usage*

To use special characters (e.g. spaces), either put the string in double quotes
or escape the individual characters:

```/usr/bin/lcd_show "%f"```

The command is executed as-is, without a shell. To redirect command output or
chain commands, wrap the command in a shell, e.g.

```
sh -c "df -P -h /recordings >/config/.markers/recording-pre-process"
sh -c "df -P -h /recordings | tee /config/.markers/recording-pre-process"
```
