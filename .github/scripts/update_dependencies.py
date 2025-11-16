#!/usr/bin/env python3
"""
Update dependency versions and SHA1 hashes in tvheadend Makefiles.
"""

import os
import re
import sys
import json
from typing import Dict, List


def update_makefile(makefile_path: str, updates: List[Dict]) -> bool:
    """Update a Makefile with new versions and SHA1 hashes."""
    if not updates:
        return False
    
    with open(makefile_path, 'r') as f:
        content = f.read()
    
    original_content = content
    
    for update in updates:
        var_prefix = update['var_prefix']
        new_version = update['new_version']
        var_type = update.get('var_type', 'ver')
        
        # Update version
        if var_type == 'name':
            # Version is in the main variable like FFMPEG = ffmpeg-6.1.1
            name_var = var_prefix
            # Construct new value based on dependency type
            if update['name'] == 'ffmpeg':
                new_value = f"ffmpeg-{new_version}"
            elif update['name'] == 'libhdhomerun':
                new_value = f"libhdhomerun_{new_version}"
            else:
                # Generic format
                new_value = f"{update['name']}-{new_version}"
            
            var_pattern = rf'^({name_var}\s*=\s*)(.+)$'
            var_replacement = rf'\g<1>{new_value}'
            content = re.sub(var_pattern, var_replacement, content, flags=re.MULTILINE)
        else:
            # Version is in a _VER variable
            ver_var = f"{var_prefix}_VER"
            ver_pattern = rf'^({ver_var}\s*=\s*)(.+)$'
            ver_replacement = rf'\g<1>{new_version}'
            content = re.sub(ver_pattern, ver_replacement, content, flags=re.MULTILINE)
        
        # Update SHA1 if available
        if update.get('sha1_computed') and update.get('new_sha'):
            sha_var = f"{var_prefix}_SHA1"
            sha_pattern = rf'^({sha_var}\s*=\s*)(.+)$'
            sha_replacement = rf'\g<1>{update["new_sha"]}'
            content = re.sub(sha_pattern, sha_replacement, content, flags=re.MULTILINE)
    
    if content != original_content:
        with open(makefile_path, 'w') as f:
            f.write(content)
        return True
    
    return False


def main():
    """Main entry point."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(script_dir))
    
    # Read updates from JSON file
    updates_file = os.path.join(repo_root, 'dependency_updates.json')
    
    if not os.path.exists(updates_file):
        print("No dependency updates file found")
        return 1
    
    with open(updates_file, 'r') as f:
        all_updates = json.load(f)
    
    if not all_updates:
        print("No updates to apply")
        return 0
    
    # Group updates by makefile
    updates_by_makefile = {}
    for update in all_updates:
        makefile = update['makefile']
        if makefile not in updates_by_makefile:
            updates_by_makefile[makefile] = []
        updates_by_makefile[makefile].append(update)
    
    # Apply updates
    modified_files = []
    for makefile_path, updates in updates_by_makefile.items():
        full_path = os.path.join(repo_root, makefile_path)
        print(f"Updating {makefile_path}...")
        
        if update_makefile(full_path, updates):
            print(f"  ✓ Updated")
            modified_files.append(makefile_path)
        else:
            print(f"  - No changes needed")
    
    print(f"\nModified {len(modified_files)} file(s)")
    
    # Output for GitHub Actions
    output_file = os.environ.get('GITHUB_OUTPUT')
    if output_file:
        with open(output_file, 'a') as f:
            f.write(f"modified_count={len(modified_files)}\n")
            f.write(f"modified_files={json.dumps(modified_files)}\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
