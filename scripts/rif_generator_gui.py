#!/usr/bin/env python3
"""
RIF Generator GUI
Graphical interface for generating PlayStation 4 RIF files for retail games
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os
import struct
import hashlib
from datetime import datetime
from pathlib import Path

class RIFGeneratorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("PlayStation 4 RIF Generator")
        self.root.geometry("600x500")
        
        # Variables
        self.content_id_var = tk.StringVar()
        self.output_dir_var = tk.StringVar(value=os.getcwd())
        self.game_title_var = tk.StringVar()
        
        self.setup_ui()
        
    def setup_ui(self):
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="PlayStation 4 RIF File Generator", 
                               font=('Arial', 16, 'bold'))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Content ID section
        ttk.Label(main_frame, text="Content ID:").grid(row=1, column=0, sticky=tk.W, pady=5)
        content_id_entry = ttk.Entry(main_frame, textvariable=self.content_id_var, width=40)
        content_id_entry.grid(row=1, column=1, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        # Game title section
        ttk.Label(main_frame, text="Game Title (optional):").grid(row=2, column=0, sticky=tk.W, pady=5)
        game_title_entry = ttk.Entry(main_frame, textvariable=self.game_title_var, width=40)
        game_title_entry.grid(row=2, column=1, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        # Output directory section
        ttk.Label(main_frame, text="Output Directory:").grid(row=3, column=0, sticky=tk.W, pady=5)
        output_dir_entry = ttk.Entry(main_frame, textvariable=self.output_dir_var, width=30)
        output_dir_entry.grid(row=3, column=1, sticky=(tk.W, tk.E), pady=5)
        
        browse_btn = ttk.Button(main_frame, text="Browse", command=self.browse_directory)
        browse_btn.grid(row=3, column=2, padx=(5, 0), pady=5)
        
        # Content ID examples
        examples_frame = ttk.LabelFrame(main_frame, text="Content ID Examples", padding="10")
        examples_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=10)
        
        examples_text = tk.Text(examples_frame, height=8, width=70)
        examples_text.grid(row=0, column=0, sticky=(tk.W, tk.E))
        
        examples_content = """Examples of PlayStation 4 Content IDs:

• EP0001-CUSA00074_00-CHILDOFLIGHT0001 (Child of Light)
• UP0001-CUSA07346_00-EAGLEFLIGHTDEMO1 (Eagle Flight Demo)
• EP0002-CUSA03529_00-GHLIVERETAILDEMO (Guitar Hero Live Demo)
• EP0006-CUSA00276_00-FIFA2014DEMOGAME (FIFA 2014 Demo)

Format: [Region]-[CUSA_ID]_[Version]-[Product_Code]
• Region: EP (Europe), UP (US), JP (Japan)
• CUSA_ID: Unique game identifier
• Version: Usually 00
• Product_Code: 16-character product identifier"""
        
        examples_text.insert(tk.END, examples_content)
        examples_text.config(state=tk.DISABLED)
        
        # Buttons frame
        buttons_frame = ttk.Frame(main_frame)
        buttons_frame.grid(row=5, column=0, columnspan=3, pady=20)
        
        generate_btn = ttk.Button(buttons_frame, text="Generate RIF File", 
                                 command=self.generate_rif, style='Accent.TButton')
        generate_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        validate_btn = ttk.Button(buttons_frame, text="Validate Content ID", 
                                 command=self.validate_content_id)
        validate_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        clear_btn = ttk.Button(buttons_frame, text="Clear", command=self.clear_fields)
        clear_btn.pack(side=tk.LEFT)
        
        # Status frame
        status_frame = ttk.LabelFrame(main_frame, text="Status", padding="10")
        status_frame.grid(row=6, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=10)
        
        self.status_text = tk.Text(status_frame, height=6, width=70)
        self.status_text.grid(row=0, column=0, sticky=(tk.W, tk.E))
        
        # Scrollbar for status
        status_scrollbar = ttk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.status_text.yview)
        status_scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        self.status_text.config(yscrollcommand=status_scrollbar.set)
        
        # Configure grid weights
        main_frame.columnconfigure(1, weight=1)
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        
    def browse_directory(self):
        directory = filedialog.askdirectory(initialdir=self.output_dir_var.get())
        if directory:
            self.output_dir_var.set(directory)
            
    def validate_content_id(self):
        content_id = self.content_id_var.get().strip()
        
        if not content_id:
            self.log_status("Please enter a Content ID")
            return False
            
        # Basic validation - Content ID format: REGION-CUSAXXXXX_XX-PRODUCTCODE
        parts = content_id.split('-')
        if len(parts) != 3:
            self.log_status("Invalid Content ID format. Expected: REGION-CUSAXXXXX_XX-PRODUCTCODE")
            self.log_status("Example: EP0001-CUSA12345_00-TESTGAMERETAIL01")
            return False
            
        region_part = parts[0]
        cusa_version_part = parts[1]
        product_part = parts[2]
        
        # Validate region (first part)
        if not region_part.startswith(('EP', 'UP', 'JP')):
            self.log_status(f"Invalid region '{region_part}'. Must start with EP, UP, or JP")
            return False
            
        # Validate CUSA and version part
        if '_' not in cusa_version_part:
            self.log_status(f"Invalid format. Missing version separator in '{cusa_version_part}'")
            return False
            
        cusa_part, version_part = cusa_version_part.split('_', 1)
        
        if not cusa_part.startswith('CUSA') or len(cusa_part) != 9:
            self.log_status(f"Invalid CUSA format '{cusa_part}'. Must be CUSAXXXXX (5 digits)")
            return False
            
        if len(version_part) != 2 or not version_part.isdigit():
            self.log_status(f"Invalid version format '{version_part}'. Must be 2 digits")
            return False
            
        # Validate product code
        if len(product_part) != 16:
            self.log_status(f"Invalid product code '{product_part}'. Must be 16 characters")
            return False
            
        self.log_status(f"Content ID '{content_id}' is valid!")
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
        rif_data[offset:offset+4] = b'RIF\x00'
        offset += 4
        
        # Version: 0x0001
        rif_data[offset:offset+2] = b'\x00\x01'
        offset += 2
        
        # Unknown field: 0xFFFF
        rif_data[offset:offset+2] = b'\xFF\xFF'
        offset += 2
        
        # Padding 1: 12 bytes of zeros
        rif_data[offset:offset+12] = b'\x00' * 12
        offset += 12
        
        # Timestamp (big-endian)
        timestamp = self.generate_timestamp(content_id)
        rif_data[offset:offset+4] = struct.pack('>I', timestamp)
        offset += 4
        
        # Padding 2: specific pattern
        rif_data[offset:offset+8] = b'\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF'
        offset += 8
        
        return bytes(rif_data)
        
    def generate_rif(self):
        content_id = self.content_id_var.get().strip()
        output_dir = self.output_dir_var.get().strip()
        game_title = self.game_title_var.get().strip()
        
        if not self.validate_content_id():
            return
            
        if not output_dir or not os.path.exists(output_dir):
            self.log_status("Please select a valid output directory")
            return
            
        try:
            # Generate RIF content
            rif_content = self.generate_rif_content(content_id)
            
            # Create filename
            filename = f"{content_id}.rif"
            filepath = os.path.join(output_dir, filename)
            
            # Write file
            with open(filepath, 'wb') as f:
                f.write(rif_content)
                
            # Log success
            self.log_status(f"Successfully generated RIF file: {filename}")
            self.log_status(f"Location: {filepath}")
            self.log_status(f"Size: {len(rif_content)} bytes")
            
            if game_title:
                self.log_status(f"Game: {game_title}")
                
            # Show timestamp info
            timestamp = self.generate_timestamp(content_id)
            timestamp_date = datetime.fromtimestamp(timestamp)
            self.log_status(f"Generated timestamp: {hex(timestamp)} ({timestamp_date})")
            
            messagebox.showinfo("Success", f"RIF file generated successfully!\n\nFile: {filename}\nLocation: {filepath}")
            
        except Exception as e:
            error_msg = f"Error generating RIF file: {str(e)}"
            self.log_status(error_msg)
            messagebox.showerror("Error", error_msg)
            
    def clear_fields(self):
        self.content_id_var.set("")
        self.game_title_var.set("")
        self.status_text.delete(1.0, tk.END)
        
    def log_status(self, message):
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.status_text.see(tk.END)
        self.root.update_idletasks()

def main():
    root = tk.Tk()
    app = RIFGeneratorGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()