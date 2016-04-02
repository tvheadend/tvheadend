const char *tvh_doc_access_entry_class[] = {
LANGPREF N_("Setting up access control is an important initial step as"),
" __",
LANGPREF N_("the system is initially wide open"),
"__ . \n\n",
LANGPREF N_("Tvheadend verifies access by scanning through all enabled access control \
entries in sequence, from the top of the list to the bottom. The \
permission flags, streaming profiles, DVR config profiles, channel tags \
and so on are combined for all matching access entries. An access entry is \
said to match if the username matches and the IP source address of the \
requesting peer is within the prefix. There is also anonymous access, if \
the user is set to asterisk. Only network prefix is matched then."),
"\n\n",

NULL
};
const char *tvh_doc_memory_information_class[] = {
LANGPREF N_("This tab displays various memory usage information useful \
for debugging, __it does not have any user configurable options__." \
"\n\n"),

NULL
};

