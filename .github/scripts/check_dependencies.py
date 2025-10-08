#!/usr/bin/env python3
"""
Check for dependency updates in tvheadend Makefiles.
This script checks GitHub releases, GitLab, and other sources for new versions.
"""

import os
import re
import sys
import json
import hashlib
import tempfile
import urllib.request
import urllib.error
import urllib.parse
from typing import Dict, List, Tuple, Optional

# Dependencies configuration
DEPENDENCIES = {
    'ffmpeg': {
        'makefile': 'Makefile.ffmpeg',
        'checks': [
            {
                'name': 'NASM',
                'var_prefix': 'NASM',
                'check_type': 'nasm_releases',
                'url': 'https://www.nasm.us/pub/nasm/releasebuilds/',
            },
            {
                'name': 'libx264',
                'var_prefix': 'LIBX264',
                'check_type': 'gitlab_commit',
                'repo': 'videolan/x264',
                'gitlab_url': 'https://code.videolan.org',
                'branch': 'master',
            },
            {
                'name': 'libx265',
                'var_prefix': 'LIBX265',
                'check_type': 'bitbucket_downloads',
                'repo': 'multicoreware/x265_git',
            },
            {
                'name': 'libvpx',
                'var_prefix': 'LIBVPX',
                'check_type': 'github_releases',
                'repo': 'webmproject/libvpx',
            },
            {
                'name': 'fdk-aac',
                'var_prefix': 'LIBFDKAAC',
                'check_type': 'github_releases',
                'repo': 'mstorsjo/fdk-aac',
            },
            {
                'name': 'nv-codec-headers',
                'var_prefix': 'FFNVCODEC',
                'check_type': 'github_releases',
                'repo': 'FFmpeg/nv-codec-headers',
            },
            {
                'name': 'ffmpeg',
                'var_prefix': 'FFMPEG',
                'var_type': 'name',  # Version is in FFMPEG variable, not FFMPEG_VER
                'check_type': 'ffmpeg_releases',
                'url': 'https://ffmpeg.org/releases/',
            },
        ]
    },
    'hdhomerun': {
        'makefile': 'Makefile.hdhomerun',
        'checks': [
            {
                'name': 'libhdhomerun',
                'var_prefix': 'LIBHDHR',
                'var_type': 'name',  # Version is in LIBHDHR variable, not LIBHDHR_VER
                'check_type': 'hdhomerun_releases',
                'url': 'https://download.silicondust.com/hdhomerun/',
            },
        ]
    }
}


def get_github_token() -> Optional[str]:
    """Get GitHub token from environment."""
    return os.environ.get('GITHUB_TOKEN')


def github_api_request(url: str) -> Dict:
    """Make a GitHub API request with authentication."""
    token = get_github_token()
    headers = {
        'Accept': 'application/vnd.github.v3+json',
    }
    if token:
        headers['Authorization'] = f'token {token}'
    
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            return json.loads(response.read())
    except urllib.error.HTTPError as e:
        print(f"HTTP Error {e.code}: {e.reason}", file=sys.stderr)
        if e.code == 403:
            print("Rate limit might be exceeded. Use GITHUB_TOKEN for higher limits.", file=sys.stderr)
        raise


def check_github_releases(repo: str) -> Optional[str]:
    """Check latest GitHub release version."""
    try:
        url = f'https://api.github.com/repos/{repo}/releases/latest'
        data = github_api_request(url)
        tag = data.get('tag_name', '')
        # Remove 'v' prefix if present
        version = tag.lstrip('v')
        return version
    except Exception as e:
        print(f"Error checking GitHub releases for {repo}: {e}", file=sys.stderr)
        return None


def check_gitlab_commit(repo: str, gitlab_url: str, branch: str = 'master') -> Optional[str]:
    """Check latest GitLab commit hash."""
    try:
        # For code.videolan.org, use their API
        api_url = f'{gitlab_url}/api/v4/projects/{urllib.parse.quote(repo, safe="")}/repository/branches/{branch}'
        headers = {'Accept': 'application/json'}
        req = urllib.request.Request(api_url, headers=headers)
        with urllib.request.urlopen(req, timeout=30) as response:
            data = json.loads(response.read())
            return data['commit']['id']
    except Exception as e:
        print(f"Error checking GitLab commit for {repo}: {e}", file=sys.stderr)
        return None


def check_ffmpeg_releases(url: str) -> Optional[str]:
    """Check latest FFmpeg release version."""
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=30) as response:
            html = response.read().decode('utf-8')
        
        # Find ffmpeg-X.Y.Z.tar.bz2 files
        pattern = r'ffmpeg-(\d+\.\d+(?:\.\d+)?).tar.bz2'
        versions = re.findall(pattern, html)
        
        if not versions:
            return None
        
        # Sort versions and return latest
        def version_key(v):
            return tuple(map(int, v.split('.')))
        
        versions.sort(key=version_key, reverse=True)
        return versions[0]
    except Exception as e:
        print(f"Error checking FFmpeg releases: {e}", file=sys.stderr)
        return None


def check_nasm_releases(url: str) -> Optional[str]:
    """Check latest NASM release version."""
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=30) as response:
            html = response.read().decode('utf-8')
        
        # Find version directories like 2.16.03/
        pattern = r'(\d+\.\d+\.\d+)/'
        versions = re.findall(pattern, html)
        
        if not versions:
            return None
        
        # Sort versions and return latest
        def version_key(v):
            return tuple(map(int, v.split('.')))
        
        versions.sort(key=version_key, reverse=True)
        return versions[0]
    except Exception as e:
        print(f"Error checking NASM releases: {e}", file=sys.stderr)
        return None


def check_hdhomerun_releases(url: str) -> Optional[str]:
    """Check latest HDHomeRun library release."""
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=30) as response:
            html = response.read().decode('utf-8')
        
        # Find libhdhomerun_YYYYMMDD.tgz files
        pattern = r'libhdhomerun_(\d{8}).tgz'
        versions = re.findall(pattern, html)
        
        if not versions:
            return None
        
        # Sort by date and return latest
        versions.sort(reverse=True)
        return versions[0]
    except Exception as e:
        print(f"Error checking HDHomeRun releases: {e}", file=sys.stderr)
        return None


def download_and_hash(url: str) -> Optional[str]:
    """Download a file and compute its SHA1 hash."""
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            tmp_path = tmp.name
        
        req = urllib.request.Request(url)
        req.add_header('User-Agent', 'tvheadend-dependency-checker')
        
        with urllib.request.urlopen(req, timeout=60) as response:
            with open(tmp_path, 'wb') as f:
                f.write(response.read())
        
        sha1 = hashlib.sha1()
        with open(tmp_path, 'rb') as f:
            while True:
                data = f.read(65536)
                if not data:
                    break
                sha1.update(data)
        
        os.unlink(tmp_path)
        return sha1.hexdigest()
    except Exception as e:
        print(f"Error downloading and hashing {url}: {e}", file=sys.stderr)
        if tmp_path and os.path.exists(tmp_path):
            os.unlink(tmp_path)
        return None


def parse_makefile(makefile_path: str) -> Dict[str, str]:
    """Parse Makefile to extract version and SHA1 variables."""
    variables = {}
    
    with open(makefile_path, 'r') as f:
        for line in f:
            line = line.strip()
            # Match variable assignments like VAR = value
            match = re.match(r'^(\w+)\s*=\s*(.+)$', line)
            if match:
                var_name, value = match.groups()
                # Don't expand variables, just store raw value
                variables[var_name] = value.strip()
    
    return variables


def expand_variable(value: str, variables: Dict[str, str]) -> str:
    """Expand $(VAR) references in a value."""
    # Simple expansion - doesn't handle nested expansions perfectly
    pattern = r'\$\((\w+)\)'
    
    def replacer(match):
        var_name = match.group(1)
        return variables.get(var_name, match.group(0))
    
    # Keep expanding until no more variables
    max_iterations = 10
    for _ in range(max_iterations):
        new_value = re.sub(pattern, replacer, value)
        if new_value == value:
            break
        value = new_value
    
    return value


def get_download_url_for_check(check: Dict, new_version: str, variables: Dict[str, str]) -> Optional[str]:
    """Construct download URL for a dependency check."""
    check_type = check['check_type']
    
    if check_type == 'github_releases':
        repo = check['repo']
        # Determine the archive extension and URL pattern
        var_prefix = check['var_prefix']
        tb_var = f"{var_prefix}_TB"
        
        # Try to infer the URL pattern from existing variables
        url_var = f"{var_prefix}_URL"
        if url_var in variables:
            url_template = variables[url_var]
            # For GitHub releases, common patterns:
            if 'archive/refs/tags' in url_template:
                # Pattern: https://github.com/REPO/archive/refs/tags/v{version}.tar.gz
                return f"https://github.com/{repo}/archive/refs/tags/v{new_version}.tar.gz"
            elif 'releases/download' in url_template:
                # Pattern: https://github.com/REPO/releases/download/v{version}/filename-{version}.tar.gz
                # Extract the filename pattern
                filename_match = re.search(r'/([^/]+)$', url_template)
                if filename_match:
                    filename_template = filename_match.group(1)
                    # Replace version in template
                    new_filename = filename_template
                    for var, val in variables.items():
                        if var.endswith('_VER') and f'$({var})' in new_filename:
                            new_filename = new_filename.replace(f'$({var})', new_version)
                    return f"https://github.com/{repo}/releases/download/n{new_version}/{new_filename}"
            elif 'archive/v' in url_template:
                # Pattern: https://github.com/REPO/archive/v{version}/filename.tar.gz
                return f"https://github.com/{repo}/archive/v{new_version}/lib{check['name']}-{new_version}.tar.gz"
        
    elif check_type == 'gitlab_commit':
        # For x264, URL is: https://code.videolan.org/videolan/x264/-/archive/{commit}/x264-{commit}.tar.bz2
        gitlab_url = check.get('gitlab_url', 'https://gitlab.com')
        repo = check['repo']
        return f"{gitlab_url}/{repo}/-/archive/{new_version}/x264-{new_version}.tar.bz2"
    
    elif check_type == 'ffmpeg_releases':
        return f"https://ffmpeg.org/releases/ffmpeg-{new_version}.tar.bz2"
    
    elif check_type == 'nasm_releases':
        return f"https://www.nasm.us/pub/nasm/releasebuilds/{new_version}/nasm-{new_version}.tar.gz"
    
    elif check_type == 'hdhomerun_releases':
        return f"https://download.silicondust.com/hdhomerun/libhdhomerun_{new_version}.tgz"
    
    return None


def check_dependency_updates(makefile_path: str, checks: List[Dict]) -> List[Dict]:
    """Check for updates to dependencies in a Makefile."""
    updates = []
    
    variables = parse_makefile(makefile_path)
    
    for check in checks:
        name = check['name']
        var_prefix = check['var_prefix']
        check_type = check['check_type']
        var_type = check.get('var_type', 'ver')  # 'ver' or 'name'
        
        # Determine which variable holds the version
        if var_type == 'name':
            # Version is in the main variable like FFMPEG = ffmpeg-6.1.1
            name_var = var_prefix
            current_full = variables.get(name_var, '')
            # Extract version from name like "ffmpeg-6.1.1" or "libhdhomerun_20210624"
            if check_type == 'ffmpeg_releases':
                # Format: ffmpeg-X.Y.Z
                match = re.search(r'-(\d+\.\d+(?:\.\d+)?)', current_full)
                current_version = match.group(1) if match else ''
            elif check_type == 'hdhomerun_releases':
                # Format: libhdhomerun_YYYYMMDD
                match = re.search(r'_(\d{8})', current_full)
                current_version = match.group(1) if match else ''
            else:
                current_version = ''
        else:
            # Version is in a separate _VER variable
            ver_var = f"{var_prefix}_VER"
            current_version = variables.get(ver_var, '')
        
        sha_var = f"{var_prefix}_SHA1"
        url_var = f"{var_prefix}_URL"
        current_sha = variables.get(sha_var, '')
        
        print(f"Checking {name}...")
        print(f"  Current version: {current_version}")
        
        # Check for new version based on type
        new_version = None
        
        if check_type == 'github_releases':
            new_version = check_github_releases(check['repo'])
        elif check_type == 'gitlab_commit':
            new_version = check_gitlab_commit(
                check['repo'],
                check.get('gitlab_url', 'https://gitlab.com'),
                check.get('branch', 'master')
            )
        elif check_type == 'ffmpeg_releases':
            new_version = check_ffmpeg_releases(check['url'])
        elif check_type == 'nasm_releases':
            new_version = check_nasm_releases(check['url'])
        elif check_type == 'hdhomerun_releases':
            new_version = check_hdhomerun_releases(check['url'])
        elif check_type == 'bitbucket_downloads':
            # Skip bitbucket for now - requires different handling
            print(f"  Skipping {name} (Bitbucket not yet supported)")
            continue
        
        if not new_version:
            print(f"  Could not determine latest version")
            continue
        
        print(f"  Latest version: {new_version}")
        
        if new_version == current_version:
            print(f"  ✓ Up to date")
            continue
        
        print(f"  ⚠ Update available: {current_version} → {new_version}")
        
        # Get download URL and compute new SHA1
        download_url = get_download_url_for_check(check, new_version, variables)
        
        if not download_url:
            print(f"  Could not determine download URL")
            updates.append({
                'name': name,
                'current_version': current_version,
                'new_version': new_version,
                'makefile': makefile_path,
                'var_prefix': var_prefix,
                'var_type': var_type,
                'sha1_computed': False
            })
            continue
        
        print(f"  Download URL: {download_url}")
        print(f"  Computing SHA1...")
        
        new_sha = download_and_hash(download_url)
        
        if not new_sha:
            print(f"  Could not compute SHA1")
            updates.append({
                'name': name,
                'current_version': current_version,
                'new_version': new_version,
                'makefile': makefile_path,
                'var_prefix': var_prefix,
                'var_type': var_type,
                'sha1_computed': False
            })
            continue
        
        print(f"  New SHA1: {new_sha}")
        
        updates.append({
            'name': name,
            'current_version': current_version,
            'new_version': new_version,
            'current_sha': current_sha,
            'new_sha': new_sha,
            'makefile': makefile_path,
            'var_prefix': var_prefix,
            'var_type': var_type,
            'sha1_computed': True
        })
    
    return updates


def main():
    """Main entry point."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(script_dir))
    
    all_updates = []
    
    for category, config in DEPENDENCIES.items():
        makefile_path = os.path.join(repo_root, config['makefile'])
        print(f"\n{'=' * 60}")
        print(f"Checking {category} dependencies in {config['makefile']}")
        print(f"{'=' * 60}")
        
        updates = check_dependency_updates(makefile_path, config['checks'])
        all_updates.extend(updates)
    
    print(f"\n{'=' * 60}")
    print("Summary")
    print(f"{'=' * 60}")
    
    if not all_updates:
        print("✓ All dependencies are up to date!")
        return 0
    
    print(f"Found {len(all_updates)} update(s):")
    for update in all_updates:
        print(f"  - {update['name']}: {update['current_version']} → {update['new_version']}")
    
    # Output JSON for GitHub Actions
    output_file = os.environ.get('GITHUB_OUTPUT')
    if output_file:
        with open(output_file, 'a') as f:
            f.write(f"updates_found={'true' if all_updates else 'false'}\n")
            f.write(f"update_count={len(all_updates)}\n")
            # Write JSON data
            f.write(f"updates={json.dumps(all_updates)}\n")
    
    # Also write to a file for the workflow to use
    updates_file = os.path.join(repo_root, 'dependency_updates.json')
    with open(updates_file, 'w') as f:
        json.dump(all_updates, f, indent=2)
    
    print(f"\nUpdate details written to: dependency_updates.json")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
