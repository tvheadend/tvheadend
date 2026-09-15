# Translation Analysis Results

## English GB vs English US Translation Comparison

This directory contains a comprehensive analysis of the differences between English GB (British English) and English US (American English) translations in Tvheadend.

### Files

1. **[TRANSLATION_ANALYSIS_EN_GB_EN_US.md](TRANSLATION_ANALYSIS_EN_GB_EN_US.md)**
   - Comprehensive analysis document with detailed explanations
   - Categories all 99 differences by type
   - Provides recommendations for each category
   - Includes examples and context

2. **[TRANSLATION_DIFFERENCES_EN_GB_EN_US.csv](TRANSLATION_DIFFERENCES_EN_GB_EN_US.csv)**
   - Spreadsheet-compatible format listing all differences
   - Columns: Category, Context (source file), Original, GB translation, US translation
   - Easy to filter and sort for review

### Quick Summary

**Total Differences Found**: 99 strings (out of thousands)

| Translation Type | Differences |
|-----------------|-------------|
| Main | 33 |
| Documentation | 59 |
| JavaScript | 7 |

### Key Findings

- **68.7%** (68 strings) are legitimate British vs American spelling differences that should be kept
  - Examples: programme/program, metre/meter, grey/gray, polarisation/polarization
  
- **11.1%** (11 strings) should be standardized across both versions
  - Examples: through/thru, pass-through/pass-thru, XML capitalization
  
- **6.1%** (6 strings) contain errors that need fixing in both versions
  - Examples: typos like "gien" → "given", "ommitted" → "omitted"
  
- **14.1%** (14 strings) have grammar improvements that need review

### Most Common Difference

The single most common difference is **programme vs program** which appears in 54 of the 99 translated strings (54.5%). This is a fundamental British vs American English spelling difference.

### Recommendation

The translations are largely appropriate. Most differences (68.7%) serve the legitimate purpose of providing proper British and American English localization. The remaining differences represent opportunities to:

1. Fix typos and errors
2. Standardize technical terminology  
3. Apply grammar improvements consistently

### Usage

To review the differences:
1. See the detailed analysis in the Markdown file for context and recommendations
2. Use the CSV file for filtering, sorting, and systematic review
3. Use the source file contexts to locate the strings in the codebase

### Translation Files Analyzed

- `intl/tvheadend.en_GB.po` vs `intl/tvheadend.en_US.po`
- `intl/docs/tvheadend.doc.en_GB.po` vs `intl/docs/tvheadend.doc.en_US.po`
- `intl/js/tvheadend.js.en_GB.po` vs `intl/js/tvheadend.js.en_US.po`
