:

Whenever you read or write data to the filesystems, the information is 
kept (cached) in memory for a while. This means that regularly-accessed 
files are available quickly without going back to the disc; it also 
means that there’s a disconnect when writing between the write request 
(from the application) and the actual write itself (to the disc/storage) 
as changes are buffered to be written in one go.

Warning, setting an incorrect scheme can lead to crashes. If you're 
unsure select *System*.

Scheme                 | Description 
-----------------------|------------
**Unknown**            | A placeholder status, meaning that the configuration isn’t properly set.
**System**             | Change nothing and rely on standard (default) system caching to behave as it normally would.
**Don't keep**         | Tell the system that you’re not expecting to re-use the data soon, so don’t keep it in cache. The data will still be buffered for writing. Useful e.g. in a RAM-limited system like a Pi (given that you’re unlikely to be watching while recording, so data can be discarded now and read back from disc later).
**Sync**               | Tell the system to write the data immediately. This doesn’t affect whether or not it’s cached. Useful e.g. if you’ve a particular problem with data loss due to delayed write (such as if you get frequent transient power problems).
**Sync + Don't keep**  | A combination of last two variants above - data is written immediately and then discarded from cache.
