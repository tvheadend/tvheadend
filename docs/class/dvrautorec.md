This tab controls EPG-driven recording rules.

!['Autorec' Tab](static/img/doc/dvrautorecentries.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

---

<tvh_include>inc/add_grid_entry</tvh_include>

If you wanted to record any programs matching "BBC News" on 
BBC One you would enter something like this into the add entry dialog: 

!['Autorec' example entry](static/img/doc/dvrautorecadd.png)

This uses a regular expression (regex) to match the program title 
"BBC News" exactly, otherwise event titles containing the phrase would 
also match, e.g "BBC News at One" and "BBC News at Six" etc.

Regular expressions examples:

Regex                             | Description
----------------------------------|------------
 `^BBC News$`                     | Matches "BBC News" exactly.
 `^(New\\: )?Regular Show$`       | Matches "Regular Show" and  (if it exists) "New: Regular Show".
 
Matching events will be added to the 
*[Upcoming/Current Recordings](class/dvrentry)* tab. 
**Note that if your rule matches any in-progress events they will 
automatically start being recorded.**

---

<tvh_include>inc/edit_grid_entries</tvh_include>

!['Edit Autorec'](static/img/doc/editautorec.png)

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

