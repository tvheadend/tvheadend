:
Example : every day at 2am is : `0 2 * * *` 

`┌───────────── min (0 - 59)` 

`│ ┌────────────── hour (0 - 23)` 

`│ │ ┌─────────────── day of month (1 - 31)` 

`│ │ │ ┌──────────────── month (1 - 12)` 

`│ │ │ │ ┌───────────────── day of week (0 - 6) (0 to 6 are Sunday to Saturday, or use names; 7 is Sunday, the same as 0)` 

`│ │ │ │ │` 

`│ │ │ │ │` 

`* * * * *` 
 
You cannot use non-standard predefined scheduling definitions for this 
field.

See [Wikipedia for a detailed look into Cron.](https://en.wikipedia.org/wiki/Cron)
