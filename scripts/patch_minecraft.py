#!/usr/bin/env python3
"""
Minecraft PS4 PSN Bypass - Complete One-Command Patcher
Extracts, patches, and creates installation-ready PKG
"""

import os
import sys
import shutil
import subprocess
import tempfile
from pathlib import Path

def run_cmd(cmd, cwd=None):
    """Run command and return success"""
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end='')
    return result.returncode == 0

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <minecraft.pkg> [output.pkg] [shadpkg_path]")
        print(f"Example: {sys.argv[0]} minecraft.pkg minecraft_patched.pkg ./shadpkg")
        return 1
    
    pkg_input = Path(sys.argv[1]).resolve()
    pkg_output = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else pkg_input.parent / "minecraft_patched.pkg"
    shadpkg = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else Path("./shadpkg").resolve()
    
    if not pkg_input.exists():
        print(f"[!] PKG not found: {pkg_input}")
        return 1
    
    if not shadpkg.exists():
        print(f"[!] shadpkg not found: {shadpkg}")
        return 1
    
    print(f"""
╔════════════════════════════════════════════════════════════════╗
║    Minecraft PS4 PSN Bypass Patcher - Complete Solution        ║
╚════════════════════════════════════════════════════════════════╝

Input PKG:  {pkg_input}
Output PKG: {pkg_output}
""")
    
    pkg_output.parent.mkdir(parents=True, exist_ok=True)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        extract_dir = tmpdir / "extracted"
        
        # Step 1: Extract
        print("[1/4] Extracting PKG...")
        extract_dir.mkdir(parents=True, exist_ok=True)
        if not run_cmd([str(shadpkg), "extract", "--input", str(pkg_input), "--export-project", str(extract_dir)]):
            print("[!] Extraction failed")
            return 1
        
        # Step 2: Scan for PSN
        print("[2/4] Scanning for PSN functions...")
        eboot = list(extract_dir.rglob("eboot.bin"))
        if not eboot:
            print("[!] eboot.bin not found")
            return 1
        
        result = subprocess.run([str(shadpkg), "patch", "--scan", "-i", str(eboot[0])], capture_output=True, text=True)
        psn_count = result.stdout.count("Found")
        print(f"[+] Found {psn_count} PSN functions/strings")
        
        # Step 3: Generate options.txt
        print("[3/4] Generating PSN bypass config...")
        result = subprocess.run([str(shadpkg), "patch", "--gen-options"], capture_output=True, text=True)
        lines = result.stdout.split('\n')
        config = []
        for line in lines:
            if line.startswith('mp_username:'):
                config.append(line)
            elif config and line.startswith('Save this content'):
                break
            elif config:
                if line.strip():
                    config.append(line)
        
        if config:
            with open(extract_dir / "options.txt", 'w') as f:
                f.write('\n'.join(config))
        
        # Step 4: Build PKG
        print("[4/4] Building patched PKG...")
        
        # Try orbis-pub-cmd
        result = subprocess.run(["which", "orbis-pub-cmd"], capture_output=True)
        if result.returncode == 0:
            cmd = ["orbis-pub-cmd", "image_build", "--image_type", "pkg", "--image_path", str(extract_dir), "--output_path", str(pkg_output)]
            if run_cmd(cmd):
                size = pkg_output.stat().st_size / (1024*1024)
                print(f"""
╔════════════════════════════════════════════════════════════════╗
║                    SUCCESS!                                    ║
╚════════════════════════════════════════════════════════════════╝

Patched PKG: {pkg_output}
Size: {size:.2f} MB

Ready to install on PS4!

NEXT STEPS:
1. Copy PKG to PS4 via USB or FTP
2. Use Package Manager to install
3. Start Minecraft and configure multiplayer
4. Copy options.txt to save data for PSN bypass

See MINECRAFT_PSN_BYPASS_README.md for detailed instructions.
""")
                return 0
        
        # If orbis-pub-cmd not available, copy extracted files
        print("[!] orbis-pub-cmd not found - creating extracted folder instead")
        pkg_extracted = pkg_output.parent / f"{pkg_output.stem}_extracted"
        if pkg_extracted.exists():
            shutil.rmtree(pkg_extracted)
        shutil.copytree(extract_dir, pkg_extracted)
        
        print(f"""
[+] Extracted files saved to: {pkg_extracted}

To create the final PKG, install LibOrbisPkg and run:
  orbis-pub-cmd image_build --image_type pkg --image_path "{pkg_extracted}" --output_path "{pkg_output}"

Or use Patch Builder GUI:
  https://www.mediafire.com/file/xw0zn2e0rjaf5k7/Patch_Builder_v1.3.3.zip/file
""")
        return 0

if __name__ == "__main__":
    sys.exit(main())
