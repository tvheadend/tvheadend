function switchtab(name, index)
{
    a = new Ajax.Updater(name + 'deck', '/ajax/' + name + 'tab/'  + index, 
			 { method: 'get', evalScripts: true});

    a = new Ajax.Updater(name + 'menu', '/ajax/' + name + 'menu/' + index,
			 { method: 'get', evalScripts: true});
};

function updatelistonserver(listid, url, resultid)
{
    //    document.getElementById(resultid).innerHTML = "Updating...";

    a = new Ajax.Updater(resultid, url, 
			 { evalScripts: true,
			   parameters:Sortable.serialize(listid)});
};

function addlistentry(listid, url, name)
{
    if(name == null || name == "") {
	alert("Emtpy name is not allowed");
    } else {
	a = new Ajax.Updater(listid, url,
			     { evalScripts: true,
			       parameters: { name: name },
			       insertion: Insertion.Bottom
			     });
    }
}


function dellistentry(url, id, name)
{
    if(confirm("Are you sure you want to delete '" + name + "'") == true) {
	a = new Ajax.Request(url, { parameters: { id: id }});
    }
}

function addlistentry_by_widget(listid, url, widget)
{
    name = $F(widget);
    $(widget).clear();
    addlistentry(listid, url, name);
}


function showhide(name)
{
    ctrlname = 'toggle_' + name;
    if(document.getElementById(ctrlname).innerHTML == 'More') {
	document.getElementById(ctrlname).innerHTML = 'Less';
	new Effect.Appear(name, {duration: 0.5});
    } else {
	document.getElementById(ctrlname).innerHTML = 'More';
	new Effect.Fade(name, {duration: 0.5});
    }
}

function tentative_chname(id, url, name)
{
	var newname = prompt("Enter name of channel", name);
	if(newname != null && newname != name) {
		a = new Ajax.Updater(id, url,
				 { evalScripts: true,
				  parameters: { newname: newname }});
	}
}

function mailboxquery(boxid)
{
	new Ajax.Request('/ajax/mailbox/' + boxid,
	{
		onFailure: function(req) { alert(req.responseText); },
		onException: function(t,e) { alert(e); }
	})
}