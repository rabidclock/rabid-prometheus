import os
import sys
import subprocess
import struct

def capture_xwd():
    # Capture the root window using xwd
    try:
        result = subprocess.run(['xwd', '-root', '-silent'], capture_output=True, check=True)
        return result.stdout
    except Exception as e:
        print(f"Error capturing XWD: {e}", file=sys.stderr)
        return None

def xwd_to_bmp(xwd_data, out_path):
    if not xwd_data:
        return False
    
    # XWD Header format (simplified)
    # The header is 100 bytes of 32-bit integers
    header = struct.unpack('>LLLLLLLLLL', xwd_data[:40])
    header_size = header[0]
    width = header[3]
    height = header[4]
    bpp = header[8] # bits per pixel
    
    if bpp != 32:
        print(f"Unsupported BPP: {bpp}", file=sys.stderr)
        return False

    # Extract pixels (skipping header)
    # XWD 32-bit is usually XRGB
    pixels = xwd_data[header_size:]
    
    # BMP Header
    file_size = 54 + (width * height * 3)
    bmp_header = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
    dib_header = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, width * height * 3, 0, 0, 0, 0)
    
    with open(out_path, 'wb') as f:
        f.write(bmp_header)
        f.write(dib_header)
        
        # BMP is stored bottom-to-top, BGR
        for y in range(height - 1, -1, -1):
            for x in range(width):
                idx = (y * width + x) * 4
                b = pixels[idx + 3]
                g = pixels[idx + 2]
                r = pixels[idx + 1]
                f.write(struct.pack('BBB', b, g, r))
            
            # Padding to 4 bytes
            padding = (4 - (width * 3) % 4) % 4
            f.write(b'\x00' * padding)
            
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: screenshot.py <out_path>")
        sys.exit(1)
        
    out_path = sys.argv[1]
    data = capture_xwd()
    if xwd_to_bmp(data, out_path):
        print(f"Screenshot saved to {out_path}")
        sys.exit(0)
    else:
        sys.exit(1)
