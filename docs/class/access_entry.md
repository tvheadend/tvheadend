Setting up access control is an important initial step as **the system
is initially wide open**. 

Tvheadend verifies access by scanning through all enabled access control
entries in sequence, from the top of the list to the bottom. The permission
flags, streaming profiles, DVR config profiles, channel tags and so on are
combined for all matching access entries. An access entry is said to match
if the username matches and the IP source address of the requesting peer
is within the prefix. There is also anonymous access, if the user is set
to asterisk. Only network prefix is matched then.

---

### Adding an Entry/Creating an Account

To create a new user, navigate to the *Configuration -> Users -> Access Entries*
tab and click on the *[Add]* button from the menu bar, then using the 
*Add Access Entry* dialog enter the *required* username and select the 
desired rights options. 

If you would like to allow anonymous access to your Tvheadend 
server you may set-up an anonymous account by entering an asterisk `*` 
in the username field. **WARNING: All access rights given to an anonymous account also
apply to subsequent accounts.**

!['Access Entry Example'](docresources/accessentriesnewuser.png)

**Don't forget** to also create a password entry for the user in the 
*Passwords* tab!

**Tips**:
* Be as limiting as possible especially when making Tvheadend available over the internet.
* For extra security, always enter (a comma-separated list of) network prefix(es) (*Allowed networks* in the *Add Access Entry* dialog).
* If you lock yourself out, you can use the backdoor account to regain access, or restart Tvheadend with the `--noacl` argument.
* You can have multiple entries using the same username with varying rights, allowing you to enable / disable each as needed. Keep in mind that matching account entry permissions are combined.

---

### Editing an Entry/Account

To edit an entry highlight (select) the entry from the grid then press 
the *[Edit]* button from the menu bar.

**Tip**: You can quickly make changes to an entry by double-clicking on 
the desired field within the grid. See *Editing Fields* on the [Web interface Guide - General](webui_general) 
page for details.

---

### Deleting a User.

To delete a user highlight (select) the entry from the grid then press
the *[Delete]* button from the menu bar.

