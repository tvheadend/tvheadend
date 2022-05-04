:

Status           | Description 
-----------------|---------------------------------------------------------- 
Aborted by user  | The recording was interrupted by the user. 
File missing     | The associated file(s) cannot be found on disk.
Time missed      | See below.

Time missed can be caused by one (or more) of the following:
* No free tuners - usually in-use by other subscription(s).
* No tuners are enabled and/or have no network assigned.
* All available tuners failed to tune (this can indicate a signal, driver or hardware problem).
* The underlying service for the channel is no longer available.
* Tvheadend wasn't running or crashed when a scheduled event/entry was to start.
  
