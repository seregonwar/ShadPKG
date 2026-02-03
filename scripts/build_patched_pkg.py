#!/usr/bin/env python3
"""
Build a patched Minecraft PKG from extracted and patched files.
This script uses Patch Builder or orbis-pub-cmd to create a valid PS4 PKG.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

def build_with_orbis_pub_cmd(extract_dir, output_pkg):
    """Build PKG using orbis-pub-cmd (LibOrbisPkg)"""
    try:
        cmd = [
            "orbis-pub-cmd",
            "image_build",
            "--image_type", "pkg",
            "--image_path", str(extract_dir),
            "--output_path", str(output_pkg)
        ]
        print(f"[*] Building PKG with orbis-pub-cmd...")
        print(f"    Command: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"[+] PKG built successfully: {output_pkg}")
            return True
        else:
            print(f"[-] orbis-pub-cmd failed:")
            print(result.stderr)
            return False
    except FileNotFoundError:
        print("[-] orbis-pub-cmd not found")
        return False

def build_with_patch_builder(extract_dir):
    """Provide instructions for building with Patch Builder GUI"""
    print("\n" + "="*70)
    print("PATCH BUILDER GUI METHOD")
    print("="*70)
    print(f"\nExtracted files are ready at:\n  {extract_dir}\n")
    print("To build the PKG using Patch Builder GUI:")
    print("1. Download Patch Builder: https://www.mediafire.com/file/xw0zn2e0rjaf5k7/Patch_Builder_v1.3.3.zip/")
    print("2. Extract and run Patch Builder")
    print("3. Select 'New Project' -> 'Patch'")
    print("4. Set the project folder to:")
    print(f"   {extract_dir}")
    print("5. Click 'Build' to create the PKG")
    print("6. The PKG will be saved in the project folder")
    print("\n" + "="*70 + "\n")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 build_patched_pkg.py <extract_dir> [output_pkg]")
        print("Example: python3 build_patched_pkg.py /tmp/shadpkg_patch /tmp/minecraft_patched.pkg")
        sys.exit(1)
    
    extract_dir = Path(sys.argv[1])
    
    if not extract_dir.exists():
        print(f"[-] Extract directory not found: {extract_dir}")
        sys.exit(1)
    
    # Default output path
    if len(sys.argv) > 2:
        output_pkg = Path(sys.argv[2])
    else:
        output_pkg = extract_dir.parent / "minecraft_patched.pkg"
    
    print(f"[*] Building patched Minecraft PKG")
    print(f"    Extract dir: {extract_dir}")
    print(f"    Output PKG:  {output_pkg}")
    
    # Try orbis-pub-cmd first
    if build_with_orbis_pub_cmd(extract_dir, output_pkg):
        print(f"\n[+] PKG ready for installation: {output_pkg}")
        sys.exit(0)
    
    # Fall back to Patch Builder instructions
    build_with_patch_builder(extract_dir)
    print("[!] Please use Patch Builder GUI to complete the PKG build")
    sys.exit(1)

if __name__ == "__main__":
    main()
