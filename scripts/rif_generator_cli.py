#!/usr/bin/env python3
"""
RIF Generator CLI
Command-line interface for generating PlayStation 4 RIF files for retail games

Usage:
    python rif_generator_cli.py <content_id> [output_directory]
    python rif_generator_cli.py --batch <file_with_content_ids> [output_directory]
    python rif_generator_cli.py --gui  # Launch GUI version

Examples:
    python rif_generator_cli.py EP0001-CUSA12345_00-TESTGAMERETAIL01
    python rif_generator_cli.py UP0001-CUSA67890_00-ANOTHERGAME12345 ./output
    python rif_generator_cli.py --batch content_ids.txt ./rif_files
"""

import sys
import os
import argparse
import struct
import hashlib
from datetime import datetime
from pathlib import Path

class RIFGenerator:
    def __init__(self):
        self.base_structure = {
            'magic': b'RIF\x00',
            'version': b'\x00\x01',
            'unknown1': b'\xFF\xFF',
            'padding1': b'\x00' * 12,
            'padding2': b'\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF'
        }
    
    def validate_content_id(self, content_id: str) -> bool:
        """Validate PlayStation 4 Content ID format"""
        if not content_id:
            return False
            
        # Basic validation - Content ID format: REGION-CUSAXXXXX_XX-PRODUCTCODE
        parts = content_id.split('-')
        if len(parts) != 3:
            print(f"Error: Invalid Content ID format. Expected: REGION-CUSAXXXXX_XX-PRODUCTCODE")
            print(f"Example: EP0001-CUSA12345_00-TESTGAMERETAIL01")
            return False
            
        region_part = parts[0]
        cusa_version_part = parts[1]
        product_part = parts[2]
        
        # Validate region (first part)
        if not region_part.startswith(('EP', 'UP', 'JP')):
            print(f"Error: Invalid region '{region_part}'. Must start with EP, UP, or JP")
            return False
            
        # Validate CUSA and version part
        if '_' not in cusa_version_part:
            print(f"Error: Invalid format. Missing version separator in '{cusa_version_part}'")
            return False
            
        cusa_part, version_part = cusa_version_part.split('_', 1)
        
        if not cusa_part.startswith('CUSA') or len(cusa_part) != 9:
            print(f"Error: Invalid CUSA format '{cusa_part}'. Must be CUSAXXXXX (5 digits)")
            return False
            
        if len(version_part) != 2 or not version_part.isdigit():
            print(f"Error: Invalid version format '{version_part}'. Must be 2 digits")
            return False
            
        # Validate product code
        if len(product_part) != 16:
            print(f"Error: Invalid product code '{product_part}'. Must be 16 characters")
            return False
            
        return True
    
    def generate_timestamp(self, content_id: str) -> int:
        """Generate a deterministic timestamp based on content ID"""
        hash_obj = hashlib.md5(content_id.encode())
        hash_int = int(hash_obj.hexdigest()[:8], 16)
        
        # Map to a reasonable timestamp range (2013-2024)
        base_timestamp = 0x52000000  # Around 2013
        max_offset = 0x10000000      # About 11 years range
        
        timestamp = base_timestamp + (hash_int % max_offset)
        return timestamp
    
    def generate_rif_content(self, content_id: str) -> bytes:
        """Generate RIF file content for a given content ID"""
        rif_data = bytearray(1024)  # Initialize 1024-byte array with zeros
        
        offset = 0
        
        # Magic number: "RIF\0"
        rif_data[offset:offset+4] = self.base_structure['magic']
        offset += 4
        
        # Version: 0x0001
        rif_data[offset:offset+2] = self.base_structure['version']
        offset += 2
        
        # Unknown field: 0xFFFF
        rif_data[offset:offset+2] = self.base_structure['unknown1']
        offset += 2
        
        # Padding 1: 12 bytes of zeros
        rif_data[offset:offset+12] = self.base_structure['padding1']
        offset += 12
        
        # Timestamp (big-endian)
        timestamp = self.generate_timestamp(content_id)
        rif_data[offset:offset+4] = struct.pack('>I', timestamp)
        offset += 4
        
        # Padding 2: specific pattern
        rif_data[offset:offset+8] = self.base_structure['padding2']
        offset += 8
        
        return bytes(rif_data)
    
    def generate_rif_file(self, content_id: str, output_path: str = None) -> bool:
        """Generate a RIF file for the given content ID"""
        if not self.validate_content_id(content_id):
            return False
            
        try:
            rif_content = self.generate_rif_content(content_id)
            
            if output_path is None:
                output_path = f"{content_id}.rif"
            elif os.path.isdir(output_path):
                output_path = os.path.join(output_path, f"{content_id}.rif")
                
            # Create output directory if it doesn't exist
            os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
            
            with open(output_path, 'wb') as f:
                f.write(rif_content)
                
            timestamp = self.generate_timestamp(content_id)
            timestamp_date = datetime.fromtimestamp(timestamp)
            
            print(f"✓ Generated RIF file: {output_path}")
            print(f"  Content ID: {content_id}")
            print(f"  Size: {len(rif_content)} bytes")
            print(f"  Timestamp: {hex(timestamp)} ({timestamp_date.strftime('%Y-%m-%d %H:%M:%S')})")
            
            return True
            
        except Exception as e:
            print(f"✗ Error generating RIF file for {content_id}: {e}")
            return False
    
    def generate_batch(self, content_ids_file: str, output_dir: str = ".") -> int:
        """Generate RIF files for multiple content IDs from a file"""
        try:
            with open(content_ids_file, 'r') as f:
                content_ids = [line.strip() for line in f if line.strip() and not line.startswith('#')]
                
            if not content_ids:
                print(f"No valid content IDs found in {content_ids_file}")
                return 0
                
            print(f"Processing {len(content_ids)} content IDs from {content_ids_file}")
            print(f"Output directory: {os.path.abspath(output_dir)}")
            print("-" * 60)
            
            success_count = 0
            for i, content_id in enumerate(content_ids, 1):
                print(f"[{i}/{len(content_ids)}] Processing: {content_id}")
                if self.generate_rif_file(content_id, output_dir):
                    success_count += 1
                print()
                
            print("-" * 60)
            print(f"Batch processing complete: {success_count}/{len(content_ids)} files generated successfully")
            
            return success_count
            
        except FileNotFoundError:
            print(f"Error: File '{content_ids_file}' not found")
            return 0
        except Exception as e:
            print(f"Error processing batch file: {e}")
            return 0

def print_examples():
    """Print usage examples"""
    examples = """
Content ID Examples:
  EP0001-CUSA00074_00-CHILDOFLIGHT0001  (Child of Light - Europe)
  UP0001-CUSA07346_00-EAGLEFLIGHTDEMO1  (Eagle Flight Demo - US)
  JP0001-CUSA12345_00-TESTGAMERETAIL01  (Test Game - Japan)

Usage Examples:
  # Generate single RIF file in current directory
  python rif_generator_cli.py EP0001-CUSA12345_00-TESTGAMERETAIL01
  
  # Generate single RIF file in specific directory
  python rif_generator_cli.py UP0001-CUSA67890_00-ANOTHERGAME12345 ./output
  
  # Generate multiple RIF files from a text file
  python rif_generator_cli.py --batch content_ids.txt ./rif_files
  
  # Launch GUI version
  python rif_generator_cli.py --gui

Batch File Format (content_ids.txt):
  # Lines starting with # are comments
  EP0001-CUSA12345_00-TESTGAMERETAIL01
  UP0001-CUSA67890_00-ANOTHERGAME12345
  JP0001-CUSA11111_00-JAPANGAMETEST001
"""
    print(examples)

def main():
    parser = argparse.ArgumentParser(
        description="PlayStation 4 RIF File Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="For more examples, use: python rif_generator_cli.py --examples"
    )
    
    parser.add_argument('content_id', nargs='?', help='PlayStation 4 Content ID')
    parser.add_argument('output_dir', nargs='?', default='.', help='Output directory (default: current directory)')
    parser.add_argument('--batch', metavar='FILE', help='Generate RIF files for content IDs listed in a file')
    parser.add_argument('--gui', action='store_true', help='Launch GUI version')
    parser.add_argument('--examples', action='store_true', help='Show usage examples')
    parser.add_argument('--validate', metavar='CONTENT_ID', help='Validate a Content ID without generating file')
    
    args = parser.parse_args()
    
    if args.examples:
        print_examples()
        return 0
    
    if args.gui:
        try:
            import subprocess
            subprocess.run([sys.executable, 'rif_generator_gui.py'])
        except Exception as e:
            print(f"Error launching GUI: {e}")
            print("Make sure rif_generator_gui.py is in the same directory")
        return 0
    
    generator = RIFGenerator()
    
    if args.validate:
        if generator.validate_content_id(args.validate):
            print(f"✓ Content ID '{args.validate}' is valid")
            return 0
        else:
            return 1
    
    if args.batch:
        success_count = generator.generate_batch(args.batch, args.output_dir)
        return 0 if success_count > 0 else 1
    
    if not args.content_id:
        parser.print_help()
        print("\nError: Content ID is required")
        print("Use --examples to see usage examples")
        return 1
    
    success = generator.generate_rif_file(args.content_id, args.output_dir)
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())