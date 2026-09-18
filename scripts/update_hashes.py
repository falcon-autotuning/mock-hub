import sys
import os
import hashlib

def sha256_file(path):
    h = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            while chunk := f.read(8192):
                h.update(chunk)
        return h.hexdigest()
    except Exception:
        return ""

def update_manifest(directory):
    manifest_path = os.path.join(directory, 'falcon.yml')
    if not os.path.exists(manifest_path):
        return

    with open(manifest_path, 'r') as f:
        lines = f.readlines()

    build_dir = os.path.join(directory, 'build')
    if not os.path.exists(build_dir):
        return

    # Find all .so files in the build directory
    so_files = [f"build/{f}" for f in os.listdir(build_dir) if f.endswith('.so')]
    if not so_files:
        return

    # Simple state machine to update/add hashes
    new_lines = []
    in_ffi = False
    processed_so = set()
    
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('ffi:'):
            in_ffi = True
            new_lines.append(line)
            continue
        
        if in_ffi:
            # Check if this line is an SO entry (matching either bare name or build/ path)
            found_so = None
            for so in so_files:
                bare_so = os.path.basename(so)
                if bare_so in stripped or so in stripped:
                    found_so = so
                    break
            
            if found_so:
                hash_val = f"sha256:{sha256_file(os.path.join(directory, found_so))}"
                new_lines.append(f"  {found_so}: {hash_val}\n")
                processed_so.add(found_so)
                continue
            
            if stripped == "" or (":" in stripped and not stripped.startswith("-") and not stripped.startswith("build/")):
                # End of FFI section or empty line
                in_ffi = False
                # Add any missing SO files before finishing FFI section
                for so in so_files:
                    if so not in processed_so:
                        hash_val = f"sha256:{sha256_file(os.path.join(directory, so))}"
                        new_lines.append(f"  {so}: {hash_val}\n")
                        processed_so.add(so)
                new_lines.append(line)
                continue
        
        new_lines.append(line)

    # If FFI section was never found, add it at the end
    if not processed_so:
        new_lines.append("\nffi:\n")
        for so in so_files:
            hash_val = f"sha256:{sha256_file(os.path.join(directory, so))}"
            new_lines.append(f"  {so}: {hash_val}\n")

    with open(manifest_path, 'w') as f:
        f.writelines(new_lines)
    print(f"  ✓ Updated hashes in {manifest_path} for files in build/")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        update_manifest(sys.argv[1])
