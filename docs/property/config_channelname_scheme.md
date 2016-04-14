: 

Note that you must correctly set the *channel icon path* (above) for 
this option to take effect/generate icon filenames. You may need to use 
the *[Reset Icon]* button in 
*Configuration -> Channel / EPG -> Channels* to trigger filename 
(re-)generation.

Scheme                 | Description
-----------------------|-------------------
No Scheme              | Use service name "as is" to generate the filename.
All lower-case         | Generate lower-case filenames.
Service name picons    | Generate lower-case filenames using picon formatting.

When using *No Scheme* or *All lower-case* - spaces are replaced with 
underscores `_` and non-alphanumeric characters are URL encoded.

When using the *service name picon scheme* - some characters will be 
replaced:

Char                   | Replacement
-----------------------|----------------------
`*`                    | star
`+`                    | plus
` ` (space)            | None (it's removed).  

