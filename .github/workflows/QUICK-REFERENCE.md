# Quick Reference: Dependency Update Workflow

## For Maintainers

### How to Use the Automated Workflow

#### Automatic Updates (Recommended)
The workflow runs automatically every Monday at 6:00 AM UTC. When updates are found:
1. A pull request is created automatically
2. Review the PR for:
   - Version changes
   - SHA1 hash verification
   - Breaking changes in dependencies
3. Test the build with updated dependencies
4. Merge if tests pass

#### Manual Trigger
To check for updates immediately:
1. Go to: **Actions** → **Check Dependency Updates**
2. Click **Run workflow**
3. Select `master` branch
4. Click **Run workflow** button

### Understanding the Pull Request

When a PR is created, it will show:
```
## Automated Dependency Updates

- **dependency-name**: `old-version` → `new-version`
  - SHA1: `new-hash`
```

### What to Review

1. **Version Changes**: Are they reasonable? (e.g., not jumping major versions unexpectedly)
2. **SHA1 Hashes**: Were they computed successfully?
3. **Build Impact**: Test the build to ensure no breaking changes
4. **Changelog**: Check upstream changelogs for breaking changes

### Testing Updates

After a PR is created:

```bash
# Checkout the PR branch
git fetch origin automated-dependency-updates
git checkout automated-dependency-updates

# Clean build
make clean

# Reconfigure if needed
./configure

# Build with updated dependencies
make -j$(nproc)

# Test the application
./build.linux/tvheadend --version
```

### Troubleshooting

#### Workflow Fails - Network Issues
- Rerun the workflow (transient failures are common)
- Check GitHub Actions logs for details

#### Workflow Fails - Invalid SHA1
- Upstream may have changed the download location
- Update `check_dependencies.py` with correct URL pattern

#### No Updates Found But One is Available
- Check if dependency is in the supported list
- Verify the check_type is correct for the source
- May need to add custom logic for that dependency

#### PR Contains Wrong Version
- Check the version extraction regex in `check_dependencies.py`
- Update the regex pattern for that dependency type

### Manual Dependency Updates

If a dependency needs manual updating:

1. Edit the appropriate Makefile (`Makefile.ffmpeg` or `Makefile.hdhomerun`)
2. Update version: `DEPNAME_VER = new-version`
3. Download the new version and compute SHA1:
   ```bash
   wget URL_TO_NEW_VERSION
   sha1sum downloaded-file.tar.gz
   ```
4. Update SHA1: `DEPNAME_SHA1 = computed-hash`
5. Commit and create PR manually

### Adding New Dependencies to Automation

To add a new dependency to automated checking:

1. Edit `.github/scripts/check_dependencies.py`
2. Add to `DEPENDENCIES` dictionary:
   ```python
   {
       'name': 'newlib',
       'var_prefix': 'NEWLIB',
       'var_type': 'ver',  # or 'name' if version in main variable
       'check_type': 'github_releases',  # or appropriate type
       'repo': 'owner/repo',  # for GitHub
   }
   ```
3. Test locally:
   ```bash
   python3 .github/scripts/check_dependencies.py
   ```
4. Commit and push changes

### Supported Check Types

- `github_releases` - GitHub releases API
- `gitlab_commit` - GitLab API (for commit tracking)
- `ffmpeg_releases` - FFmpeg download page scraping
- `nasm_releases` - NASM download page scraping
- `hdhomerun_releases` - HDHomeRun download page scraping
- `bitbucket_downloads` - Not yet implemented (manual only)

## For Developers

### Script Locations
- **Checker**: `.github/scripts/check_dependencies.py`
- **Updater**: `.github/scripts/update_dependencies.py`
- **Workflow**: `.github/workflows/dependency-updates.yml`

### Local Testing
```bash
# Check for updates (dry run)
python3 .github/scripts/check_dependencies.py

# Review the generated update file
cat dependency_updates.json

# Apply updates to Makefiles (if you want to test)
python3 .github/scripts/update_dependencies.py

# Revert changes if needed
git checkout Makefile.ffmpeg Makefile.hdhomerun
```

### Running Tests
```bash
# Syntax validation
python3 -m py_compile .github/scripts/*.py

# YAML validation
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/dependency-updates.yml'))"

# Integration test
python3 << 'EOF'
import sys
sys.path.insert(0, '.github/scripts')
from check_dependencies import parse_makefile
vars = parse_makefile('Makefile.ffmpeg')
assert 'NASM_VER' in vars
print("✓ Tests passed")
EOF
```

## Quick Commands

```bash
# View current dependency versions
grep -E "_VER|^FFMPEG |^LIBHDHR " Makefile.ffmpeg Makefile.hdhomerun

# Check when workflow last ran
gh workflow view "Check Dependency Updates" --yaml

# View workflow runs
gh run list --workflow=dependency-updates.yml

# Trigger workflow manually
gh workflow run dependency-updates.yml

# View logs from last run
gh run view --log
```

## Support

For issues or questions:
- Check logs in GitHub Actions tab
- Review `.github/workflows/IMPLEMENTATION-SUMMARY.md` for technical details
- Review `.github/workflows/README-dependency-updates.md` for usage guide
- Open an issue with logs and description of the problem
