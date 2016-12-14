Setting up access control is an important initial step as **the system
is initially wide open**. 

Tvheadend verifies access by scanning through all enabled access control
entries in sequence, from the top of the list to the bottom. The permission
flags, streaming profiles, DVR config profiles, channel tags, channel
number ranges are combined for all matching access entries if allowed using
the change flag. If the parameter is empty (permission
flags, all types of profiles, channel tags and ranges) in an access
control entry and the parameter change flag is turned on,
the parameter (value, list or range) is cleared (unset).

An access entry is said to match if the username matches and the IP
source address of the requesting peer is within the prefix. There is also
anonymous access, if the user is set to asterisk. Only network prefix is
matched then.

*The order of entries is really important!* It is recommended to put the
wildcards on top of the entries and the special permissions to the bottom.

!['Access Entries Grid'](static/img/doc/accessentriesgrid.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

Entries are checked in order (when logging in, etc), the following 
functions allows you to change the ordering:

Button                 | Function
-----------------------|---------
**Move Up**            | Move the selected entry up the grid.
**Move Down**          | Move the selected entry down the grid. 

---

<tvh_include>inc/add_grid_entry</tvh_include>

####Example

This is an example of a limited user account entry.

!['Access Entry Example'](static/img/doc/accessentriesnewuser.png)

Remember to also create a password entry for the user in the 
*[Passwords](class/passwd)* tab!

**Tips**:
* Be as limiting as possible especially when making Tvheadend available 
over the Internet.
* For extra security always enter (a comma-separated list of) 
network prefix(es) (*Allowed networks*).
* You can have multiple entries using the same username with varying 
rights, allowing you to enable / disable each as needed. Note, matching 
(enabled) accounts will have permissions combined.

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

###Emergency/Backdoor Access

Tvheadend includes functionality that allows you to regain access to 
your Tvheadend instance in case of emergency or if you find yourself 
locked out, this is known as a superuser account. On some systems you 
may been asked to enter a superuser username and password during 
installation.

To create a superuser account you must have access to your Tvheadend 
configuration directory (most commonly `$HOME/.hts/tvheadend`) and 
be able to create a plain-text file named `superuser` with the following 
(JSON formatted) content:

```
{
"username": "superuser",
"password": "superpassword"
}
```

Once you've created this file you must restart Tvheadend for it to take 
affect. Note that for security the superuser account is not listed in the 
access entries grid.

**Tip**: Remember to set the correct permissions so that Tvheadend 
is able to read the superuser file.

---

###Anonymous Access

If you would like to allow anonymous access to your Tvheadend server 
you may set-up a wildcard account, you can do this by creating a new 
user and entering an asterisk `*` in the username field.

**WARNING**: Permissions given to a wildcard account apply 
to **all** accounts.

---
