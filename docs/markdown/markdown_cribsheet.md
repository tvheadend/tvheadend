## Markdown/Formatting Crib Sheet

Some notable items about how formatting is used on this particular site.

In general, **keep it simple**, especially if you're contributing to the
pages that get carried over into the web help. The simpler the formatting,
the cleaner the conversion, the less tidying up there is afterwards.

### References

* Markdown basics: [GitHub mastering markdown](https://guides.github.com/features/mastering-markdown)

### Including Documentation/Items

You can include documentation/items in other markdown 
files by using the tvh_class_doc, tvh_include and tvh_class_items tags.

For example to include the passwd items you'd enter something like this:

```
<tvh_class_items>passwd</tvh_class_items>
``` 

To include class documentation you'd use:

```
<tvh_class_doc>passwd</tvh_class_doc>
```

To include multi-use docs (placed in the `docs/markdown/inc/` folder:

```
<tvh_include>inc/common_button_table_end</tvh_include>
```

### Paragraphs Versus Definition Lists

Watch this one - indentation is key.

**This is paragraph formatting**
: with a subsequent explanation

    **This is paragraph formatting**
    : with a subsequent explanation

**This is definition list formatting**
:   with a subsequent explanation

    **This is definition list formatting**
    :   with a subsequent explanation

They may render the same here, but note the extra leading spaces in the 
second example: this means that they will convert differently for use in 
the web interface help. That in turn means your formatting will be all
over the place unless you handle the dl/dt/dd formatting in Tvheadend's CSS. 

Stick to paragraph formatting unless and until you have a need for 
definition lists.

### Lists

Mixed lists don't work without further python extensions. Be careful.

```markdown
1. First ordered list item
2. Another item
  * Unordered sub-list. 
1. Actual numbers don't matter, just that it's a number
  1. Ordered sub-list
4. And another item.

   You can't have have properly indented paragraphs within list items. 
   
* Unordered list can use asterisks
- Or minuses
+ Or pluses

Oh, and

77. Each numbered (ordered) list will restart from 1.
```

... produces:

1. First ordered list item
2. Another item
  * Unordered sub-list. 
1. Actual numbers don't matter, just that it's a number
  1. Ordered sub-list
4. And another item.

   You can't have have properly indented paragraphs within list items. 
   
* Unordered list can use asterisks
- Or minuses
+ Or pluses

Oh, and

77. Each numbered (ordered) list will restart from 1.

### Tables

Tables can be constructed as follows.

The markup code:

    First Header                | Second Header
    --------------------------- | -------------
    Content from cell 1         | Content from cell 2
    Content in the first column | Content in the second column

Will generate:

First Header                | Second Header
--------------------------- | -------------
Content from cell 1         | Content from cell 2
Content in the first column | Content in the second column

And if you don't want a header, you can leave it out - but the cells
remain in this theme, so I'd suggest you don't do this as it's ugly:

     | 
    --------------------------- | -------------
    Headless table cell 1       | Content from cell 2
    Content in the first column | Content in the second column

 | 
--------------------------- | -------------
Headless table cell 1       | Content from cell 2
Content in the first column | Content in the second column

We're using default heading/cell justification, so it's consistent throughout.
