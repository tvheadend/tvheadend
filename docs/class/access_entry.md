Setting up access control is an important initial step as **the system
is initially wide open**. 

Tvheadend verifies access by scanning through all enabled access control
entries in sequence, from the top of the list to the bottom. The permission
flags, streaming profiles, DVR config profiles, channel tags and so on are
combined for all matching access entries. An access entry is said to match
if the username matches and the IP source address of the requesting peer
is within the prefix. There is also anonymous access, if the user is set
to asterisk. Only network prefix is matched then.

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

This is an example of a very limited user account entry.

!['Access Entry Example'](static/img/doc/accessentriesnewuser.png)

If you would like to allow anonymous access to your Tvheadend 
server you may set-up an anonymous account by entering an asterisk `*` 
in the username field. **WARNING: All access rights given to an 
anonymous account also apply to subsequent accounts.**

**Don't forget** to also create a password entry for the user in the 
*[Passwords](class/passwd)* tab!

**Tips**:
* Be as limiting as possible especially when making Tvheadend available 
over the Internet.
* For extra security always enter (a comma-separated list of) 
network prefix(es) (*Allowed networks*).
* If you lock yourself out, you can use the [backdoor account](#emergency-backdoor-access) to regain 
access, or restart Tvheadend with the `--noacl` argument.
* You can have multiple entries using the same username with varying 
rights, allowing you to enable / disable each as needed. Keep in mind 
that matching account entry permissions are combined (enabled entries only).
* If you create an anonymous account, it also requires 
a [password](class/passwd) entry (enter an asterisk `*` for both the 
username and password fields when adding the entry).

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

###Emergency/Backdoor Access

Tvheadend includes functionality that allows you to regain access to 
your Tvheadend instance in case of emergency or if you find yourself 
locked out, this is known as a superuser account. On some systems you 
might been asked to enter a superuser username and password during 
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
