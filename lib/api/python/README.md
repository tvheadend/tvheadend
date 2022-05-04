tvh-json.py
=================================
(c) 2017 Tvheadend Foundation CIC

The json import / export tool written in the python language.


Background
----------

TVHeadend uses internal database called idnode. This database uses
unique identifier (UUID) for each entry. The entries may be linked
(thus some nodes may refer to another). The each entry belongs
to a class which defines the meta data (type, description).


Environment variables
---------------------

Name          | Description
--------------|--------------------------------------------
TVH_API_URL   | URL like http://localhost:9981/api
TVH_USER      | username for HTTP API
TVH_PASS      | password for HTTP API


Command 'get'
-------------

This is basic command which allows the elementary operation through
the HTTP API. Basically, almost all other commands uses this internally.

Arguments:
* path
* json string (arguments)


Command 'pathlist'
------------------

Returns all path names.


Command 'classes'
-----------------

Returns all class names.


Command 'import'
----------------

Imports the json data and updates the internal structure in TVHeadend's
database. The structure must be identical to data printed with the 'export'
command.

Arguments:
* uuid
* filename (optional - otherwise standard input is used)


Command 'export'
----------------

Exports the json data from TVHeadend's database.

Arguments:
* uuid


Command 'exportcls'
-------------------

Exports the json data from TVHeadend's database for all idnodes which
belongs to the specified class.

Arguments:
* class


Examples:
---------

`TVH_USER=admin TVH_PASS=admin ./tvh-json.py exportcls dvb_mux_dvbs`

`TVH_USER=admin TVH_PASS=admin ./tvh-json.py export 6dbdb849bd8eae9c1fecf684f773baca > a.json`

`TVH_USER=admin TVH_PASS=admin ./tvh-json.py import 6dbdb849bd8eae9c1fecf684f773baca < a.json`
