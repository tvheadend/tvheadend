
#Full-Text vs Merge-Text Searching Options

The 'Merge-text' search option provides enhancements to the existing 'Full-text' search option.  Both options test the regular expression provided against an EPG event's Title, Sub-title (short description), Summary, Description, Credits and Keywords.  If both options are selected, only the 'Merge-text' search will be performed.

The 'Full-text' option will test each of the above-mentioned fields one-by-one in isolation.  Alternately, the 'Merge-text' option will test all of the above-mentioned fields as a single merged field consisting of values for all of the fields in all of the available languages for the EPG entry in question.

When merging fields, each field is prefixed with a specific code so that search terms can be applied to a specific field.

##Merge-text Field Prefixes

The following field prefixes are used:

Prefix                          | Field
--------------------------------|------------------------------
0x01|Title
0x02|Subtitle (Short Description)
0x03|Summary
0x04|Description
0x05|Credits
0x06|Keywords
0x07|End
0x09|Field separator (Tab)

####Note: In addition to the field prefixes, a special field separator is provided between individual language elements within a field.

Field prefixes are always provided in the same sequence and will be present even if the field is empty.

##Sample Data:

[0x01][0x09]en[0x09]Event Title[0x09]fr[0x09]Titre de l'événement[0x02][0x09]en[0x09]Event Sub-Title[0x09]fr[0x09]Sous-titre de l'événement[0x03][0x04][0x05][0x06][0x07]

This sample shows an EPG record having an English title of 'Event Title' and a French title of 'Titre de l'événement' as well as an English sub-title of 'Event Sub-Title' and a French sub-title of 'Sous-titre de l'événement'.  No other fields contain any data.

####Note: The '[' and ']' characters are only used for illustrative purposes, they are not present in the actual data.

Caution: On systems with constrained resources, Merge-text searches should be used with caution due to the extra system load and overheads required to perform the search.

##Usage Example:

"Find all EPG events whose title contains 'big bang theory' where 'leonard' is mentioned in the sub-title or summary or description, but 'sheldon' is not."

``(?<=\\x01).*big bang theory.*(?=\\x02).*?(?<=\\x02)(?!.*sheldon).*leonard.*(?=\\x05)``

Confining the first criteria 'big bang theory' to be between a ``\\x01`` and a ``\\x02`` restricts matches to text within the 'Title' field.  Confining the second criteria to be in between a ``\\x02`` and a ``\\x05`` restricts matches to text in the merged 'Sub-title', 'Summary' or 'Description' fields.

