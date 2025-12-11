import sys
import os

def convert_pem_to_cpp(filename, var_name="root_ca"):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        return

    print(f"\n// Copy the following block into your source code:")
    print(f"const char* {var_name} = \\")
    
    for i, line in enumerate(lines):
        # Remove trailing newline characters from the file line
        clean_line = line.rstrip()
        if not clean_line:
            continue
            
        # Format as C string literal with newline char
        # Example: "-----BEGIN CERTIFICATE-----\n"
        print(f'    "{clean_line}\\n"')

    print("    ;")
    print("// End of certificate\n")

if __name__ == "__main__":
    print("--- PEM to C++ String Converter ---")
    
    # 1. Get filename from arguments or input
    if len(sys.argv) > 1:
        fpath = sys.argv[1]
    else:
        fpath = input("Enter path to PEM file (e.g., cert.pem): ").strip()

    # 2. Get variable name (optional)
    vname = "root_ca"
    if len(sys.argv) > 2:
        vname = sys.argv[2]
    
    # 3. Convert
    if fpath:
        # Check if user stripped quotes from drag-and-drop
        fpath = fpath.strip("'").strip('"')
        convert_pem_to_cpp(fpath, vname)
    else:
        print("No file provided.")