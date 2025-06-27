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
`%s`   | Program subtitle                          |  Afternoon
`%p`   | Program episode                           |  S02.E07
`%A`   | Program season number                     |  2
`%B`   | Program episode number                    |  7
`%d`   | Program description                       |  News and stories…
`%e`   | Error message                             |  Aborted by user
`%S`   | Start time stamp of recording, UNIX epoch |  1224421200
`%E`   | Stop time stamp of recording, UNIX epoch  |  1224426600
`%r`   | Number of errors during recording         |  0
`%R`   | Number of data errors during recording    |  6
`%i`   | Streams (comma separated)                 |  H264,AC3,TELETEXT

*Example usage*

To use special characters (e.g. spaces), either put the string in double quotes
or escape the individual characters.

```/usr/bin/tvh_file_removed "%f"```

The command is executed as-is, without a shell. To redirect command output or
chain commands, wrap the command in a shell, e.g.

```
sh -c "df -P -h /recordings >/config/.markers/recording-post-remove"
sh -c "df -P -h /recordings | tee /config/.markers/recording-post-remove"
```
