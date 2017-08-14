:

The *Change parameters* flag allows you to control which parameters
(permission flags, all types of profiles, channel tags and ranges) are
combined when multiple entries match a username/login. When the
change parameter flag is enabled (checked) for a parameter that
isn't set, the next matching entry (if any) in the sequence can set it.

Username/Entry (matched in sequence)   | "Change parameters" "Rights" checked?     | Admin enabled/set?  | Can access configuration tab?
---------------------------------------|-------------------------------------------|---------------------|------------------------------
* (Anon entry).                        | Yes.                                      | No.                 | No.
* (Another anon entry).                | No.                                       | Yes.                | Yes, this is because the above entry "Change parameters" rights **is** checked and this user has admin rights.
John doe.                              | Yes.                                      | Yes.                | No, this is because the above entry "Change parameters" rights **isn't** checked even though this user has admin rights.
John doe (Another entry).              | Yes.                                      | No.                 | No, this is because the even though above entry "Change parameters" rights **is** checked this user has no admin rights.

Option                     | Description/Properties
---------------------------|---------------------------
**Rights**                 | *Streaming*, *Web interface*, *Video recorder* (DVR), *Admin* and *Anonymize HTSP access*.
**Channel number range**   | *Minimal channel number* and *Maximal channel number*.
**Channel tags**           | *Exclude channel tags* and *Channel tags*.
**DVR configurations**     | *DVR configuration profiles*.
**Streaming profiles**     | *Streaming profiles*.
**Connection limits**      | *Connection limit type* and *Limit connections*.
**Language**               | *Language*.
**Web interface language** | *Web interface language*.
**Theme**                  | *Theme*.
**User interface level**   | *User interface level*.

The above table displays the *Change parameters* option name and the fields that it 
applies to, as shown in add/edit dialog(s).
