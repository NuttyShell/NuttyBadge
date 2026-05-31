#!/usr/bin/env python3

ORIGINAL_FILE="badge (Copy).kicad_pcb"
PATCHED_FILE="badge (Copy).kicad_pcb"

# WARNING: Make sure use size 0.6, drill 0.3 stitches

# If patched is True and patched line is empty, whole line will not be written to the output file
# If patched is False, regardless the original line is empty or not, it will still be written to the output file
def patch(line):
    # Only remove (via (at XXX XXX) (size 0.6) (drill 0.3) (layers "F.Cu" "B.Cu") (free) (net 2) (tstamp XXX))
    if "(via (" in line and '(size 0.6) (drill 0.3) (layers "F.Cu" "B.Cu") (free) (net 2) ' in line:
        return ('', True)
    return (None, False)


with open(ORIGINAL_FILE, "r") as f:
    lines = f.readlines()
    print(f"[+] Read {len(lines)} lines from {ORIGINAL_FILE}.")

with open(PATCHED_FILE, "w") as f:
    patched_count = 0
    for line in lines:
        (patched_line, patched) = patch(line)
        if patched is True:
            patched_count += 1
            if len(patched_line) > 0:
                f.write(patched_line)
            else:
                pass
                # Not writing a patched empty line
        else:
            f.write(line)

print(f"[+] All done: Patched {patched_count} line(s) and written to {PATCHED_FILE}")
