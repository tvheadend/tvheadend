# Dependency Update Workflow

This workflow automatically checks for updates to tvheadend's upstream dependencies and creates pull requests when updates are found.

## Features

- **Scheduled checks**: Runs weekly on Monday at 6:00 AM UTC
- **Manual triggering**: Can be triggered manually from the GitHub Actions tab
- **Automatic PR creation**: Creates a pull request with updated versions and SHA1 hashes
- **Multiple dependency sources**: Supports GitHub releases, GitLab commits, FFmpeg releases, NASM releases, and HDHomeRun releases

## Supported Dependencies

### FFmpeg Dependencies (Makefile.ffmpeg)
- **NASM**: Netwide Assembler
- **libx264**: H.264 video encoder
- **libx265**: H.265/HEVC video encoder (Bitbucket - manual checks only)
- **libvpx**: VP8/VP9 video codec
- **libogg**: Ogg container format
- **libtheora**: Theora video codec
- **libvorbis**: Vorbis audio codec
- **fdk-aac**: Fraunhofer FDK AAC codec
- **libopus**: Opus audio codec
- **nv-codec-headers**: NVIDIA codec headers
- **ffmpeg**: FFmpeg multimedia framework

### HDHomeRun Dependencies (Makefile.hdhomerun)
- **libhdhomerun**: HDHomeRun library

## How It Works

1. **Check Phase**: The workflow runs `check_dependencies.py` which:
   - Parses the Makefile to extract current versions and SHA1 hashes
   - Checks upstream sources for new versions using appropriate APIs
   - Downloads new versions and computes SHA1 hashes
   - Generates a JSON file with update information

2. **Update Phase**: If updates are found, `update_dependencies.py`:
   - Updates the Makefile with new versions and SHA1 hashes
   - Uses regex patterns to preserve Makefile formatting

3. **PR Creation**: A pull request is created with:
   - Detailed list of updated dependencies
   - Old and new versions
   - SHA1 hashes for verification
   - Automated labels for categorization

## Manual Usage

You can run the scripts manually from the repository root:

```bash
# Check for updates
python3 .github/scripts/check_dependencies.py

# Apply updates (if dependency_updates.json exists)
python3 .github/scripts/update_dependencies.py
```

## Limitations

- **Bitbucket dependencies**: libx265 from Bitbucket is not automatically checked (requires manual updates)
- **Rate limiting**: GitHub API has rate limits; workflow uses GITHUB_TOKEN for higher limits
- **Network access**: Some dependency sources may be blocked or require special handling

## Configuration

To modify the dependency checks, edit `.github/scripts/check_dependencies.py` and update the `DEPENDENCIES` dictionary.

## Workflow File

The workflow is defined in `.github/workflows/dependency-updates.yml`.
