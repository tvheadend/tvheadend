: Command to run after finishing a recording. The command will be run in
  background and is executed even if a recording is aborted or an error
  occurred. Use the %e error formatting string to check for errors, the
  error string is “OK” if recording finished successfully.

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
`%g`   | Program content type                      |  Current affairs
`%e`   | Error message                             |  Aborted by user
`%S`   | Start time stamp of recording, UNIX epoch |  1224421200
`%E`   | Stop time stamp of recording, UNIX epoch  |  1224426600
`%r`   | Number of errors during recording         |  0
`%R`   | Number of data errors during recording    |  6
`%i`   | Streams (comma separated)                 |  H264,AC3,TELETEXT
`%U`   | Unique ID of recording                    |  3cf44328eda87a428ba9a8b14876ab80
`%Z`   | Comment                                   |  A string

*Example usage*

To use special characters (e.g. spaces), either put the string in double quotes
or escape the individual characters.

```/path/to/ffmpeg -i "%f" -vcodec libx264 -acodec copy "/path/with white space/%b"```

The command is executed as-is, without a shell. To redirect command output or
chain commands, wrap the command in a shell, e.g.

```
sh -c "df -P -h /recordings >/config/.markers/recording-post-process"
sh -c "df -P -h /recordings | tee /config/.markers/recording-post-process"
```
