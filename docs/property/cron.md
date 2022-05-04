:
Example : every day at 2am is : `0 2 * * *` 

```
# * * * * *
# ┬ ┬ ┬ ┬ ┬
# │ │ │ │ │
# │ │ │ │ │
# │ │ │ │ └───── day of week (0 - 6 or Sunday - Saturday)
# │ │ │ └────────── month (1 - 12)
# │ │ └─────────────── day of month (1 - 31)
# │ └──────────────────── hour (0 - 23)
# └───────────────────────── min (0 - 59)
```
 
You cannot use non-standard predefined scheduling definitions for this 
field.

See [Wikipedia for a detailed look into Cron.](https://en.wikipedia.org/wiki/Cron)
