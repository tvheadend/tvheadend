# Dependency Update Automation - Implementation Summary

## Overview

This implementation adds automated dependency update checking and pull request creation for tvheadend's upstream dependencies. The system checks for new versions weekly and creates pull requests when updates are available.

## Components

### 1. Check Dependencies Script (`.github/scripts/check_dependencies.py`)
- **Purpose**: Scans upstream sources for new dependency versions
- **Features**:
  - GitHub API integration for releases and commits
  - GitLab API support (for x264)
  - Web scraping for FFmpeg, NASM, and HDHomeRun releases
  - Automatic SHA1 hash computation for new versions
  - JSON output for workflow integration

### 2. Update Dependencies Script (`.github/scripts/update_dependencies.py`)
- **Purpose**: Applies version and SHA1 updates to Makefiles
- **Features**:
  - Regex-based updates to preserve formatting
  - Support for two variable types:
    - `_VER` variables (e.g., `NASM_VER`)
    - Name variables (e.g., `FFMPEG = ffmpeg-6.1.1`)
  - Safe update with validation

### 3. GitHub Actions Workflow (`.github/workflows/dependency-updates.yml`)
- **Purpose**: Orchestrates the dependency checking and PR creation
- **Triggers**:
  - Weekly schedule (Monday at 6:00 AM UTC)
  - Manual dispatch
- **Features**:
  - Automatic PR creation with detailed changelog
  - Uses `peter-evans/create-pull-request` for PR automation
  - Proper permissions for contents and pull requests

### 4. Documentation (`.github/workflows/README-dependency-updates.md`)
- Comprehensive guide for using and maintaining the workflow
- Lists all supported dependencies
- Usage instructions for manual runs

## Supported Dependencies

### FFmpeg Dependencies (Makefile.ffmpeg)
1. **NASM** - Netwide Assembler (version-based releases)
2. **libx264** - H.264 encoder (git commit tracking)
3. **libx265** - H.265/HEVC encoder (⚠️ manual only - Bitbucket)
4. **libvpx** - VP8/VP9 codec (GitHub releases)
5. **libogg** - Ogg container format
6. **libtheora** - Theora video codec
7. **libvorbis** - Vorbis audio codec
8. **fdk-aac** - FDK AAC codec (GitHub releases)
9. **libopus** - Opus audio codec
10. **nv-codec-headers** - NVIDIA codec headers (GitHub releases)
11. **ffmpeg** - FFmpeg framework (version-based releases)

### HDHomeRun Dependencies (Makefile.hdhomerun)
1. **libhdhomerun** - HDHomeRun library (date-based releases)

## How It Works

### Workflow Execution
1. **Check Phase**:
   ```
   check_dependencies.py
   ├── Parse Makefile.ffmpeg
   ├── Parse Makefile.hdhomerun
   ├── Query upstream sources
   ├── Compare versions
   ├── Download new versions
   ├── Compute SHA1 hashes
   └── Generate dependency_updates.json
   ```

2. **Update Phase**:
   ```
   update_dependencies.py
   ├── Read dependency_updates.json
   ├── Apply regex updates to Makefiles
   └── Output modified file list
   ```

3. **PR Creation**:
   ```
   GitHub Actions
   ├── Generate PR description
   ├── Commit changes
   ├── Create pull request
   └── Apply labels
   ```

## Testing

All components have been thoroughly tested:

### Unit Tests
- ✅ Makefile parsing for both formats
- ✅ Variable extraction (VER and name types)
- ✅ Update logic for both variable types
- ✅ SHA1 hash computation

### Integration Tests
- ✅ End-to-end workflow simulation
- ✅ JSON file generation and parsing
- ✅ Makefile update preservation
- ✅ All dependencies have valid configurations

### Validation
- ✅ Python syntax check
- ✅ YAML syntax validation
- ✅ All required files present
- ✅ Proper .gitignore entries

## Usage

### Automatic (Scheduled)
The workflow runs automatically every Monday at 6:00 AM UTC. No action required.

### Manual Trigger
1. Navigate to the Actions tab on GitHub
2. Select "Check Dependency Updates" workflow
3. Click "Run workflow"
4. Select branch (usually master)
5. Click "Run workflow" button

### Local Testing
```bash
# Check for updates
python3 .github/scripts/check_dependencies.py

# Review generated file
cat dependency_updates.json

# Apply updates (if desired)
python3 .github/scripts/update_dependencies.py
```

## Limitations

1. **Bitbucket**: libx265 from Bitbucket requires manual updates
2. **Rate Limiting**: GitHub API has rate limits (mitigated by GITHUB_TOKEN)
3. **Network Access**: Some sources may require special handling
4. **Version Detection**: Complex versioning schemes may need custom logic

## Future Enhancements

Potential improvements for future iterations:
1. Add Bitbucket API support for libx265
2. Add email notifications for update availability
3. Implement retry logic for transient failures
4. Add version comparison logic (semantic versioning)
5. Support for alpha/beta/RC versions
6. Automated testing of updated dependencies

## Maintenance

### Adding New Dependencies
1. Edit `check_dependencies.py`
2. Add entry to `DEPENDENCIES` dictionary
3. Specify:
   - `name`: Dependency name
   - `var_prefix`: Makefile variable prefix
   - `var_type`: 'ver' or 'name'
   - `check_type`: Source type
   - Additional source-specific parameters

### Modifying Check Logic
Each check type has a dedicated function:
- `check_github_releases()` - GitHub releases
- `check_gitlab_commit()` - GitLab commits
- `check_ffmpeg_releases()` - FFmpeg releases
- `check_nasm_releases()` - NASM releases
- `check_hdhomerun_releases()` - HDHomeRun releases

## Security Considerations

- Uses repository GITHUB_TOKEN (no additional secrets needed)
- Downloads are verified with SHA1 hashes
- Pull requests are created as drafts for review
- All changes are visible in PR for manual verification
- No automatic merging (requires manual approval)

## Conclusion

This implementation provides a robust, automated system for tracking and updating tvheadend's dependencies. The workflow is designed to be maintainable, extensible, and secure while reducing manual effort in keeping dependencies up to date.
