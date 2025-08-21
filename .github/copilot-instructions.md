# Tvheadend

Tvheadend is the leading TV streaming server and Digital Video Recorder for Linux written in C. It provides a web-based interface and supports various TV input sources including DVB, ATSC, IPTV, HDHomeRun, and SAT>IP.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Working Effectively

### Bootstrap and Build the Repository
- Install build dependencies:
  ```bash
  sudo apt update && sudo apt install -y gettext pkg-config build-essential libavahi-client-dev libssl-dev zlib1g-dev wget bzip2 git-core liburiparser-dev python3 python3-requests ca-certificates cmake libpcre2-dev libdvbcsa-dev
  ```
- Configure the build:
  ```bash
  ./configure --disable-ffmpeg_static --disable-libav --disable-hdhomerun_static --disable-pcloud_cache
  ```
- Build tvheadend: 
  ```bash
  make -j$(nproc)
  ```
  **NEVER CANCEL: Build takes 20-30 seconds on modern hardware. ALWAYS set timeout to 10+ minutes to be safe.**

### Run the Application
- ALWAYS run the bootstrapping steps first before attempting to run tvheadend.
- Start tvheadend with first-run setup (creates default admin user with no password):
  ```bash
  ./build.linux/tvheadend -C --http_port 9981
  ```
- Access web interface at: `http://localhost:9981` (if running with port forwarding)
- Run with different configuration directory:
  ```bash
  ./build.linux/tvheadend -c /path/to/config --http_port 9981
  ```
- Settings are stored in `$HOME/.hts/tvheadend` by default

### Available Build Options
- The configure script has 74+ options. Key ones for development:
  - `--enable-ccdebug` - Enable debug build (no optimization)  
  - `--disable-pie` - Disable position independent executable
  - `--disable-ffmpeg_static` - Don't build static ffmpeg (requires internet)
  - `--disable-libav` - Disable libav/ffmpeg support entirely
  - `--disable-hdhomerun_static` - Don't build static HDHomeRun libs
  - `--disable-pcloud_cache` - Disable pcloud caching system
- View all options: `./configure --help`

## Validation

### ALWAYS Test Basic Functionality After Changes
- Verify binary was built: `ls -la build.linux/tvheadend`
- Test help output: `./build.linux/tvheadend --help`
- Test version: `./build.linux/tvheadend --version` 
- Test startup (10 second timeout): `timeout 10s ./build.linux/tvheadend -C --nostderr --http_port 9981`
- For UI changes: Start the server and access the web interface to validate functionality

### Test Scripts and Debugging
- EIT scrape regex testing: `python3 support/eitscrape_test.py data/conf/epggrab/eit/scrape/[country] support/testdata/eitscrape/[country]`
- Enable debug output: `./build.linux/tvheadend --debug [subsystem] --stderr`
- List debug subsystems: `./build.linux/tvheadend --subsystems`
- Enable trace: `./build.linux/tvheadend --trace [subsystem]`

### Build Validation Requirements
- ALWAYS run build validation after making code changes
- Test that tvheadend starts without crashes
- If making webui changes, verify the web interface loads properly at http://localhost:9981

## Installation and Packaging

### System Installation
- Install to system: `sudo make install` (installs to /usr/local by default)
- Install with custom prefix: `sudo make install prefix=/opt/tvheadend`
- Uninstall: `sudo make uninstall`

### Docker Development
- Build development container:
  ```bash
  docker image build --rm --tag 'tvheadend:builder' --target 'builder' ./
  ```
  **Note**: This may fail due to Alpine package differences - use native build for development.
- Run in development container:
  ```bash
  docker container run --interactive --rm --tty --user "$(id -u):$(id -g)" --volume "$(pwd):/workdir" --workdir '/workdir' 'tvheadend:builder' '/bin/sh'
  ```
- Then inside container: `./configure && make`

## Common Tasks

### Build System Understanding
- Primary build file: `Makefile` (includes Makefile.common)
- Configure system: `configure` script (uses support/configure.inc)
- Web UI build: `Makefile.webui` (builds compressed JS/CSS)
- Documentation build: Converts markdown docs to C code during build
- Build output directory: `build.linux/` (contains tvheadend binary and build artifacts)

### Key Source Directories
- `src/` - Main C source code
- `src/webui/` - Web interface source (ExtJS based)
- `src/api/` - REST API implementation  
- `src/input/` - TV input source implementations
- `src/epggrab/` - Electronic Program Guide grabbing
- `data/` - Configuration files and channel lists
- `docs/` - Documentation in markdown format
- `support/` - Build and utility scripts

### Development Workflow
- Always build after making changes: `make -j$(nproc)`
- Clean build when needed: `make clean && make -j$(nproc)`  
- Reconfigure if configure.ac changes: `./configure [options]`
- For webui changes, the web assets are rebuilt automatically during make

### Build Timing Expectations
- **NEVER CANCEL builds or long-running commands**
- Clean build: 20-25 seconds (set timeout to 10+ minutes minimum)  
- Incremental build: 1-2 seconds (set timeout to 5+ minutes minimum)
- Configure: 2-3 seconds
- Installation: < 1 second
- Clean operation: < 1 second

### Critical Configuration Notes
- Internet access is required for static library builds (ffmpeg, hdhomerun, x264, etc.)
- In restricted environments, disable static builds: `--disable-ffmpeg_static --disable-hdhomerun_static`
- Core functionality works without ffmpeg/libav but transcoding features are disabled
- DVB functionality requires libdvbcsa-dev package
- Web UI requires gzip and bzip2 for asset compression

### Repository Root Contents
```
.github/          - GitHub workflows and configuration
Autobuild/        - Automated build scripts  
Containerfile     - Docker build configuration
Makefile*         - Build system files
README.md         - Project overview and basic build instructions
configure         - Configuration script (74+ options available)
data/             - Default configuration and channel data
debian/           - Debian packaging files
docs/             - Documentation source (markdown)
intl/             - Internationalization files
lib/              - External libraries and Python modules
man/              - Manual pages
src/              - C source code (main application)
support/          - Build scripts and utilities
vendor/           - Third-party code
```

### Common Build Issues and Solutions
- **Missing gettext**: Install with `sudo apt install gettext`
- **Missing pkg-config**: Install with `sudo apt install pkg-config build-essential`
- **Download failures during build**: Add `--disable-ffmpeg_static --disable-hdhomerun_static --disable-pcloud_cache` to configure
- **Permission errors**: Ensure user has write access to build directory
- **Outdated configure**: Re-run `./configure` if build.linux/.config.mk is older than configure script
