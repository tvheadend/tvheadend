:

Option           | Description
-----------------|------------
**Light**        | Use the (default) light theme.
**Dark**         | Use the dark theme, a low-glare palette for night-time use.
**Auto**         | Follow the browser or operating system, switching between **Light** and **Dark** as that setting changes.
**Access**       | Use the high contrast accessibility theme.

**Auto** tracks the host's `prefers-color-scheme`, so the interface
follows a system that switches itself at sunset without the theme
needing to be changed by hand. The switch takes effect as soon as the
host setting changes; no reload is needed.

**Dark** and **Auto** style the Vue web interface only. The legacy
ExtJS interface has no dark stylesheet of its own and falls back to its
light one when either theme is selected.

This setting can be overridden on a per-user basis, see [Access Entries](class/access).
