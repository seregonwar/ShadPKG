#!/usr/bin/env python3
"""
RIF File Analyzer and Generator
Analyzes PlayStation 4 RIF (Rights Information File) structure
and generates dynamic RIF files for retail games
"""

import os
import struct
import hashlib
from typing import Dict, List, Tuple
from pathlib import Path

class RIFAnalyzer:
    def __init__(self):
        self.rif_structure = {
            'magic': b'RIF\x00',  # 4 bytes: "RIF" + null terminator
            'version': b'\x00\x01',  # 2 bytes: version (0x0001)
            'unknown1': b'\xFF\xFF',  # 2 bytes: unknown field
            'padding1': b'\x00' * 12,  # 12 bytes: padding/reserved
            'timestamp_offset': 20,  # Offset where timestamp starts
            'padding2_pattern': b'\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF'  # 8 bytes pattern
        }
        
    def analyze_rif_file(self, filepath: str) -> Dict:
        """Analyze a RIF file and extract its structure"""
        with open(filepath, 'rb') as f:
            data = f.read()
            
        if len(data) != 1024:
            raise ValueError(f"Invalid RIF file size: {len(data)} bytes (expected 1024)")
            
        analysis = {
            'filepath': filepath,
            'size': len(data),
            'magic': data[0:4],
            'version': data[4:6],
            'unknown1': data[6:8],
            'padding1': data[8:20],
            'timestamp': struct.unpack('>I', data[20:24])[0],  # Big-endian 32-bit timestamp
            'padding2': data[24:32],
            'content_id': self.extract_content_id(filepath),
            'header_hex': data[0:32].hex().upper()
        }
        
        return analysis
    
    def extract_content_id(self, filepath: str) -> str:
        """Extract content ID from filename"""
        filename = os.path.basename(filepath)
        if '_' in filename:
            return filename.split('_')[0] + '_' + filename.split('_')[1]
        return filename.replace('.rif', '')
    
    def analyze_directory(self, directory: str) -> List[Dict]:
        """Analyze all RIF files in a directory"""
        results = []
        rif_files = Path(directory).rglob('*.rif')
        
        for rif_file in rif_files:
            try:
                analysis = self.analyze_rif_file(str(rif_file))
                results.append(analysis)
                print(f"Analyzed: {rif_file.name}")
            except Exception as e:
                print(f"Error analyzing {rif_file}: {e}")
                
        return results
    
    def find_patterns(self, analyses: List[Dict]) -> Dict:
        """Find common patterns across RIF files"""
        patterns = {
            'common_magic': set(),
            'common_version': set(),
            'common_unknown1': set(),
            'timestamp_range': [],
            'common_padding2': set()
        }
        
        for analysis in analyses:
            patterns['common_magic'].add(analysis['magic'])
            patterns['common_version'].add(analysis['version'])
            patterns['common_unknown1'].add(analysis['unknown1'])
            patterns['timestamp_range'].append(analysis['timestamp'])
            patterns['common_padding2'].add(analysis['padding2'])
            
        patterns['timestamp_range'] = (min(patterns['timestamp_range']), max(patterns['timestamp_range']))
        
        return patterns

class RIFGenerator:
    def __init__(self):
        self.base_structure = {
            'magic': b'RIF\x00',
            'version': b'\x00\x01',
            'unknown1': b'\xFF\xFF',
            'padding1': b'\x00' * 12,
            'padding2': b'\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF'
        }
    
    def generate_timestamp(self, content_id: str) -> int:
        """Generate a deterministic timestamp based on content ID"""
        # Create a hash of the content ID and use it to generate a timestamp
        hash_obj = hashlib.md5(content_id.encode())
        hash_int = int(hash_obj.hexdigest()[:8], 16)
        
        # Map to a reasonable timestamp range (2013-2024)
        base_timestamp = 0x52000000  # Around 2013
        max_offset = 0x10000000      # About 11 years range
        
        timestamp = base_timestamp + (hash_int % max_offset)
        return timestamp
    
    def generate_rif_content(self, content_id: str) -> bytes:
        """Generate RIF file content for a given content ID"""
        # Start with base structure
        rif_data = bytearray(1024)  # Initialize 1024-byte array with zeros
        
        # Fill in the known structure
        offset = 0
        
        # Magic number
        rif_data[offset:offset+4] = self.base_structure['magic']
        offset += 4
        
        # Version
        rif_data[offset:offset+2] = self.base_structure['version']
        offset += 2
        
        # Unknown field
        rif_data[offset:offset+2] = self.base_structure['unknown1']
        offset += 2
        
        # Padding 1
        rif_data[offset:offset+12] = self.base_structure['padding1']
        offset += 12
        
        # Timestamp (big-endian)
        timestamp = self.generate_timestamp(content_id)
        rif_data[offset:offset+4] = struct.pack('>I', timestamp)
        offset += 4
        
        # Padding 2
        rif_data[offset:offset+8] = self.base_structure['padding2']
        offset += 8
        
        # Fill remaining bytes with pattern or zeros
        # The rest of the file appears to be mostly zeros or encrypted data
        # For a basic generator, we'll leave it as zeros
        
        return bytes(rif_data)
    
    def generate_rif_file(self, content_id: str, output_path: str) -> bool:
        """Generate a RIF file for the given content ID"""
        try:
            rif_content = self.generate_rif_content(content_id)
            
            with open(output_path, 'wb') as f:
                f.write(rif_content)
                
            print(f"Generated RIF file: {output_path}")
            return True
            
        except Exception as e:
            print(f"Error generating RIF file: {e}")
            return False

def main():
    # Analyze existing RIF files
    analyzer = RIFAnalyzer()
    rif_directory = r"c:\Users\marco\source\repos\TEST-SC\ShadPKG\gift\RIF Files"
    
    print("Analyzing existing RIF files...")
    analyses = analyzer.analyze_directory(rif_directory)
    
    print(f"\nAnalyzed {len(analyses)} RIF files")
    
    # Find patterns
    patterns = analyzer.find_patterns(analyses)
    print("\nCommon patterns found:")
    print(f"Magic: {patterns['common_magic']}")
    print(f"Version: {patterns['common_version']}")
    print(f"Unknown1: {patterns['common_unknown1']}")
    print(f"Timestamp range: {hex(patterns['timestamp_range'][0])} - {hex(patterns['timestamp_range'][1])}")
    
    # Example: Generate a new RIF file
    generator = RIFGenerator()
    
    # Test with a retail game content ID
    test_content_id = "EP0001-CUSA12345_00-TESTGAMERETAIL01"
    output_file = f"{test_content_id}.rif"
    
    print(f"\nGenerating test RIF file for: {test_content_id}")
    success = generator.generate_rif_file(test_content_id, output_file)
    
    if success:
        print(f"Test RIF file generated successfully: {output_file}")
        
        # Analyze the generated file to verify structure
        test_analysis = analyzer.analyze_rif_file(output_file)
        print(f"Generated file analysis:")
        print(f"  Size: {test_analysis['size']} bytes")
        print(f"  Magic: {test_analysis['magic']}")
        print(f"  Version: {test_analysis['version']}")
        print(f"  Timestamp: {hex(test_analysis['timestamp'])}")
        print(f"  Header: {test_analysis['header_hex']}")

if __name__ == "__main__":
    main()