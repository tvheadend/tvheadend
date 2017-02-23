:

Action              | Description
--------------------|------------
**NONE**            | No action, may be used for the logging and a comparison verification.
**USE**             | Use this elementary stream.
**ONE\_TIME**       | Use this elementary stream only one time per service type (like video,   audio, subtitles) and language. The first sucessfully compared rule wins. For example, when one AC3 elementary stream is marked to be used with ‘eng’ language and another rule with the ONE\_TIME action was   matched, the new AC3 elementary stream will not be added if the language for new AC3 elementary stream is ‘eng’. Note that the second rule might not have the language filter (column) set.   For the CA filter, this rule means that the new CA elementary stream is added only if another CA is not already used. 
**EXCLUSIVE**       | Use only this elementary stream. No other elementary streams will be used.
**EMPTY**           | Add this elementary stream only when no elementary streams are used from previous rules. It does not match the implicit USE rules which are added after the user rules.
**IGNORE**          | Ignore this elementary stream. This stream is not used. Another successfully compared rule with different action may override it.
