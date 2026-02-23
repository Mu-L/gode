
import sys
import os

try:
    import pefile
except ImportError:
    print("pefile not installed. Please install it with 'pip install pefile'")
    sys.exit(1)

def list_imports(dll_path):
    print(f"\n--- Dependencies for: {dll_path} ---")
    if not os.path.exists(dll_path):
        print(f"File not found: {dll_path}")
        return

    try:
        pe = pefile.PE(dll_path)
        if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
            print("No imports found.")
            return
            
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            print(f"  {entry.dll.decode()}")
    except Exception as e:
        print(f"Error reading {dll_path}: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        for path in sys.argv[1:]:
            list_imports(path)
    else:
        print("Usage: python check_deps.py <dll_path>...")
