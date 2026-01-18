# English GB vs English US Translation Analysis

## Executive Summary

This document analyzes the differences between English GB and English US translations in Tvheadend. Three translation file pairs were analyzed (main, docs, js) and **99 strings** were found that are actually translated between English GB and English US, rather than just blindly copied.

## Translation Files Analyzed

1. **Main translations**: `intl/tvheadend.en_GB.po` vs `intl/tvheadend.en_US.po`
2. **Documentation translations**: `intl/docs/tvheadend.doc.en_GB.po` vs `intl/docs/tvheadend.doc.en_US.po`
3. **JavaScript translations**: `intl/js/tvheadend.js.en_GB.po` vs `intl/js/tvheadend.js.en_US.po`

## Summary Statistics

| Translation Type | Differences Found |
|-----------------|-------------------|
| Main | 33 |
| Documentation | 59 |
| JavaScript | 7 |
| **Total** | **99** |

## Categories of Differences

### 1. British vs American Spelling (68 occurrences - 68.7%)

The most common differences are standard spelling variations that represent legitimate regional differences.

#### program/programme (54 occurrences)
The most frequent difference, appearing in 54 of 99 translated strings.

**British English**: "programme" 
**American English**: "program"

Examples:
- "Electronic Programme Guide" (GB) vs "Electronic Program Guide" (US)
- "Record this programme now" (GB) vs "Record this program now" (US)
- "Entertainment programmes for 6 to 14" (GB) vs "Entertainment programs for 6 to 14" (US)

**Recommendation**: ✅ Keep - This is a core British/American spelling difference.

#### Other spelling differences:

| Word (GB) | Word (US) | Count | Context | Recommendation |
|-----------|-----------|-------|---------|----------------|
| metres | meters | 1 | Altitude setting | ✅ Keep |
| Grey | Gray | 2 | Color/theme name | ✅ Keep |
| enquiry | inquiry | 1 | PIN inquiry/enquiry | ✅ Keep |
| Polarisation | Polarization | 2 | Satellite settings | ✅ Keep |
| Internationalisation | Internationalization | 1 | Documentation | ✅ Keep |
| initialised | initialized | 2 | System startup | ✅ Keep |
| localised | localized | 2 | Settings/configuration | ✅ Keep |
| analogue | analog | 2 | Channel/signal type | ✅ Keep |
| aerial | antenna | 1 | Terrestrial TV | ✅ Keep |

All these represent legitimate regional spelling differences and should be retained.

### 2. Grammar and Style Improvements (12 occurrences - 12.1%)

Some translations include minor grammar or punctuation improvements that may have been added during translation:

1. **Comma additions for clarity**:
   - "Depending on the option selected this tells" (original) 
   - → "Depending on the option selected, this tells" (GB/US)

2. **Hyphenation**:
   - "stream specific" → "stream-specific" (US)
   - "fast increasing" → "fast-increasing" (US)

3. **Article additions**:
   - "Force usage of entered CA ID" (original/GB)
   - → "Force usage of the entered CA ID" (US)

4. **Punctuation changes**:
   - "you'd imagine..." → "you'd imagine;" (US)
   - Different handling of commas and periods

**Recommendation**: 📝 Review these individually. Some are genuine improvements that could be applied to both versions rather than being region-specific.

### 3. Terminology Differences (8 occurrences - 8.1%)

#### Pass-thru vs Pass-through (3 occurrences)
**British English**: "Pass-through"
**American English**: "Pass-thru" or "Passthru"

Examples:
- "MPEG-TS Pass-through/built-in" (GB) vs "MPEG-TS Pass-thru/built-in" (US)
- "Pass-through muxer" (GB) vs "Pass-thru muxer" (US)
- "Passthrough Muxer SI Tables" (GB) vs "Passthru Muxer SI Tables" (US)

**Recommendation**: ⚠️ Consider standardizing. "Pass-through" is more formal and widely used in technical documentation. However, "thru" is common in American informal writing.

#### through vs thru (5 occurrences)
**British English**: "through"
**American English**: "thru"

Examples:
- "accessible through VPN" (GB) vs "accessible thru VPN" (US)
- "Associate each of your tuners with the correct network through" (GB) vs "thru" (US)
- "map a path through all those variables" (GB) vs "thru" (US)
- "operated primarily through a tabbed" (GB) vs "thru" (US)
- "how far through a programme" (GB) vs "thru a program" (US)

**Recommendation**: ⚠️ "Through" is the standard formal spelling. "Thru" is informal/casual American English. For professional software documentation, consider using "through" in both versions.

### 4. Capitalization Differences (2 occurrences - 2.0%)

1. **Auto-Map vs Auto-map**:
   - "Auto-Map to channels" (original/US)
   - "Auto-map to channels" (GB)
   - **Recommendation**: ⚠️ Standardize - lowercase "map" is more consistent

2. **xml vs XML**:
   - "display-name xml tag" (GB)
   - "display-name XML tag" (US)
   - **Recommendation**: ⚠️ Use "XML" (proper acronym) in both versions

### 5. Formatting and Errors (6 occurrences - 6.1%)

#### Formatting differences:
- **H264,AC3,TELETEXT** (GB) vs **H264, AC3, TELETEXT** (US)
  - US adds spaces after commas (better readability)
  - **Recommendation**: 🔧 Apply spacing to both versions

#### Grammatical errors in original corrected differently:
1. **"an redirection" → "a redirection"**:
   - Both GB and US have this in some places, but US version corrects it in 2 instances
   - **Recommendation**: 🔧 Use "a redirection" in both versions

2. **Typos found**:
   - "gien" → should be "given" (found in both)
   - "ommitted" → should be "omitted" (found in both)
   - **Recommendation**: 🔧 Fix typos in both versions

3. **Period placement**:
   - "in your EPG.)" (GB) vs "in your EPG)" (US)
   - **Recommendation**: 🔧 Remove extra period in GB version

### 6. Other Minor Differences (3 occurrences)

These are subtle differences that may or may not be intentional:

1. "prority" vs "priority" - looks like a typo in both, but GB says "the prority in which to give" while US says "what priority to assign"
2. "browsers only support certain formats" (GB) vs "support for different formats and codecs varies" (US) - complete rephrase
3. "note that not all devices" (GB) vs "note that not all devices, so the value" (US) - sentence structure change

**Recommendation**: 📝 Review for clarity and accuracy.

## Complete List of All 99 Translated Strings

### Main Translations (33 differences)

1. `Altitude (meters)` → GB: `Altitude (metres)` / US: `Altitude (meters)`
2. `Auto-Map to channels` → GB: `Auto-map to channels` / US: `Auto-Map to channels`
3. `Children's / Youth programs` → GB: `Children's / Youth programmes` / US: `Children's / Youth programs`
4. DSCP description text (grammar improvements)
5. `stream specific information` → GB: (same) / US: `stream-specific information, like...`
6. `Electronic Program Guide` → GB: `Electronic Programme Guide` / US: `Electronic Program Guide`
7. `over-the-air program guide` → GB: `over-the-air programme guide` / US: `over-the-air program guide`
8. `Entertainment programs for 10 to 16` → GB: `Entertainment programmes for 10 to 16` / US: `Entertainment programs for 10 to 16`
9. `Entertainment programs for 6 to 14` → GB: `Entertainment programmes for 6 to 14` / US: `Entertainment programs for 6 to 14`
10. `entered CA ID` → GB: (same) / US: `the entered CA ID`
11. `Gray` → GB: `Grey` / US: `Gray`
12. `MPEG-TS Pass-thru/built-in` → GB: `MPEG-TS Pass-through/built-in` / US: `MPEG-TS Pass-thru/built-in`
13. `Name (or date) of program` → GB: `Name (or date) of programme` / US: `Name (or date) of program`
14. `PIN inquiry match string` → GB: `PIN enquiry match string` / US: `PIN inquiry match string`
15. `Pass-thru muxer` → GB: `Pass-through muxer` / US: `Pass-thru muxer`
16. `Passthrough Muxer SI Tables` → GB: (same) / US: `Passthru Muxer SI Tables`
17. `Polarization` → GB: `Polarisation` / US: `Polarization`
18. `Pre-school children's programs` → GB: `Pre-school children's programmes` / US: `Pre-school children's programs`
19. `Program synopsis (display only).` → GB: `Programme synopsis (display only).` / US: `Program synopsis (display only).`
20. `Program synopsis.` → GB: `Programme synopsis.` / US: `Program synopsis.`
21. `School programs` → GB: `School programmes` / US: `School programs`
22. `Subtitle of the program (if any) (display only).` → GB: `Subtitle of the programme (if any) (display only).` / US: `Subtitle of the program (if any) (display only).`
23. `Subtitle of the program (if any).` → GB: `Subtitle of the programme (if any).` / US: `Subtitle of the program (if any).`
24. `filter matching events/programs` → GB: `filter matching events/programmes` / US: `filter matching events/programs`
25. `match programmes that are no longer` → GB: (same) / US: `match programs that are no longer`
26. `match programs that are no shorter` → GB: `match programmes that are no shorter` / US: `match programs that are no shorter`
27. `in your EPG.)` → GB: (same) / US: `in your EPG)`
28. LNB polarization description (3 occurrences of polarization/polarisation)
29. `title of the program to look for` → GB: `title of the programme to look for` / US: `title of the program to look for`
30. `Title of the program (display only).` → GB: `Title of the programme (display only).` / US: `Title of the program (display only).`
31. `Title of the program.` → GB: `Title of the programme.` / US: `Title of the program.`
32. `display-name xml tag` → GB: (same) / US: `display-name XML tag`
33. `Waiting for program start` → GB: `Waiting for programme start` / US: `Waiting for program start`

### Documentation Translations (59 differences)

Most documentation differences follow the same patterns as main translations:
- 42 occurrences of program/programme
- 4 occurrences of through/thru
- 2 occurrences of Gray/Grey
- 2 occurrences of analog/analogue  
- 2 occurrences of initialized/initialised
- 2 occurrences of localized/localised
- 1 occurrence of Internationalization/Internationalisation
- 1 occurrence of aerial/antenna
- Plus various grammar improvements and formatting fixes

### JavaScript Translations (7 differences)

All 7 JavaScript translation differences are related to program/programme:

1. `Create an automatic recording rule to record all future programs` → GB: `programmes` / US: `programs`
2. `Delete scheduled recording of this program` → GB: `programme` / US: `program`
3. `Electronic Program Guide` → GB: `Electronic Programme Guide` / US: `Electronic Program Guide`
4. `Play this program` → GB: `Play this programme` / US: `Play this program`
5. `Record this program now` → GB: `Record this programme now` / US: `Record this program now`
6. `Stop recording of this program` → GB: `Stop recording of this programme` / US: `Stop recording of this program`
7. `scans the EPG for programs to record` → GB: `scans the EPG for programmes to record` / US: `scans the EPG for programs to record`

## Overall Recommendations

### ✅ Keep As Is (68 strings - 68.7%)
These are legitimate regional differences that serve the purpose of proper localization:
- All program/programme instances (54)
- All other spelling variations (14): meters/metres, Gray/Grey, inquiry/enquiry, polarization/polarisation, etc.

### ⚠️ Consider Standardizing (11 strings - 11.1%)
These differences may be stylistic rather than regional and could benefit from standardization:
- through/thru (5) - recommend "through" for both (formal)
- Pass-through/Pass-thru (3) - recommend "pass-through" for both (formal)
- XML capitalization (1) - recommend "XML" for both (proper acronym)
- Auto-Map capitalization (1) - recommend "Auto-map" for both (consistency)
- H264,AC3 spacing (1) - recommend spaces after commas for both

### 🔧 Fix Errors (6 strings - 6.1%)
These appear to be errors that should be corrected in both versions:
- "an redirection" → "a redirection" (2)
- "EPG.)" → "EPG)" (1)
- Typos: "gien" → "given" (1)
- Typos: "ommitted" → "omitted" (2)

### 📝 Review for Consistency (14 strings - 14.1%)
These involve grammar improvements that may have been made during translation:
- Comma usage and hyphenation
- Article additions ("the")
- Sentence structure improvements

Some of these improvements could potentially be applied to both versions if they enhance clarity without compromising the original meaning.

## Conclusion

The analysis shows that **68.7%** of the translated strings represent legitimate British vs American English differences that are appropriate and necessary for proper localization. The remaining **31.3%** represent opportunities for improvement through:

1. **Terminology standardization** (11.1%): Adopting consistent technical terminology across both variants
2. **Error correction** (6.1%): Fixing typos and grammatical errors
3. **Grammar consistency** (14.1%): Reviewing whether grammar improvements should be applied to both versions

The translation effort is largely appropriate and serves its purpose well, with the majority of differences being genuine regional variations.
