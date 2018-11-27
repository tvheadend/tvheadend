## Updating the Documentation 

The documentation is written in markdown, and then converted for
direct inclusion to tvheadend binary. The markdown processor in
Tvheadend binary adds other information from the internal
class system.

The User Guide
in [documentatation repository](https://github.com/tvheadend/tvheadend-documentation)
fetches the markdown files using the build-in web server and use
them as source for mkdocs.

### Instructions For Built-in Help

Change markdown files in `docs/markdown`, `docs/markdown/inc`, `docs/class`, `docs/wizard`, etc.
Images are placed in `src/webui/static/img/doc/`.

Build Tvheadend as you normally would, see the [development page](https://tvheadend.org/projects/tvheadend/wiki/Development) for details.

