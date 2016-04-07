The *status* column indicates why an entry failed:

Status           | Description 
-----------------|---------------------------------------------------------- 
Aborted by user  | The recording was interrupted by the user. 
File missing     | The associated file(s) cannot be found on disk.
Time missed      | Indicates a recording failed due to one (or more) of the following: Tvheadend wasn't running when the entry/recording was scheduled to start; Tvheadend couldn't create the necessary file(s)/folder(s) to begin recording due to file system issues; The tuner returned no data indicating a signal/external problem; No free tuner was available for use;
