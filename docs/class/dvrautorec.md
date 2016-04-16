This tab controls EPG-driven recording rules.

!['Autorec' Tab](docresources/dvrautorecentries.png)

---

###Buttons

The tab has the following buttons:

Button                 | Function
-----------------------|-------------------
**Save**               | Save any changes made to the grid/entries.
**Undo**               | Revert any changes made since the last save.
**Add**                | Display the *Add Autorec* dialog.
**Delete**             | Delete the selected entry/entries.
**Edit**               | Edit the selected entry.
**Help**               | Display this help page.

---

###Adding an Entry/Rule

You can add an entry by pressing the *[Add]* button from the menu bar.

For example, if you wanted to record any programs matching "BBC News" on 
BBC One you would enter something like this into the add entry dialog: 

!['Autorec' example entry](docresources/dvrautorecadd.png)

This uses a regular expression (regex) to match the program title 
"BBC News" exactly, otherwise event titles containing the phrase would 
also match (e.g "BBC News at One", "BBC News at Six" etc).

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

###Editing an Entry

Highlight (select) the desired entry within the grid then click the *[Edit]* 
button from the menu bar.

**Tip**: You can quickly make changes to an entry by double-clicking on 
the desired field within the grid. See *Editing Fields* on the [Web interface Guide - General](webui_general) 
page for details.

---

###Deleting an Entry

Highlight (select) the desired entry within the grid then click the *[Delete]* 
button from the menu bar.

---
