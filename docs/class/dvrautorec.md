This tab controls EPG-driven recording rules.

!['Autorec' Tab](docresources/dvrautorecentries.png)

---

###Adding an Entry/Rule

You can add an entry by pressing the *[Add]* button from the menu bar.

For example, if you wanted to record any programs matching "BBC News" on 
BBC One you would enter something like this into the add entry dialog: 

!['Autorec' example entry](docresources/dvrautorecadd.png)

This uses a regular expression (regex) to match the program title 
"BBC News" exactly, otherwise event titles containing the phrase would also 
match (e.g "BBC News at One", "BBC News at Six" etc).

Regular expressions examples:

Regex                             | Description
----------------------------------|------------
 `^BBC News$`                     | Matches "BBC News" exactly.
 `^(New\\: )?Regular Show$`       | Matches "Regular Show" and (if it exists) "New: Regular Show".
 
EPG events matched by an autorec rule will appear as a [DVR Entry](class/dvrentry), this 
includes events that are already in-progress.

###Editing an Entry

Highlight (select) the desired entry within the grid then click the *[Edit]* 
button from the menu bar.

**Tip**: You can quickly make changes to an entry by double-clicking on 
the desired field within the grid. See *Editing Fields* on the [Web interface Guide - General](webui_general) 
page for details.

###Deleting an Entry

Highlight (select) the desired entry within the grid then click the *[Delete]* 
button from the menu bar.
