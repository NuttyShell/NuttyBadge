#!/usr/bin/env python3
"""
ssc_to_nnr.py — Convert a Stepmania .sm or .ssc chart to NutNutRevolution .nnr binary.

Usage:
    python ssc_to_nnr.py input.sm  [output.nnr]
    python ssc_to_nnr.py input.ssc [output.nnr]
    python ssc_to_nnr.py "SONG.nnr"           # works even if named .nnr but is SM/SSC text

The output .nnr file is placed next to the input file unless [output.nnr] is given.

NNR binary layout
-----------------
Header  76 B : "NNR1"(4)  title(32)  artist(32)  bpm(4f)  offset_ms(2h)  num_diffs(1B)  pad(1B)
Diff    16 B : name(8)  meter(1B)  pad(1B)  note_count(2H)  data_offset(4I)
Note     3 B : delta_ms(2H little-endian)  columns(1B)
               delta_ms = ms since previous note (or since t=0 for first note)
               columns  bit0=L  bit1=D  bit2=U  bit3=R

Note times are in ms from audio start (not from chart beat-0).
"""

import sys, os, re, struct

MAGIC       = b'NNR1'
HEADER_SZ   = 76
DIFF_SZ     = 16
MAX_DIFFS   = 8
MAX_DELTA   = 65535   # uint16_t ceiling — notes >65 s apart get a silent spacer event


# ── tag helpers ────────────────────────────────────────────────────────────────

def get_tag(text, name):
    """Return the value of the FIRST matching #NAME:value; tag, stripped."""
    m = re.search(r'#' + name + r'\s*:(.*?);', text, re.IGNORECASE | re.DOTALL)
    return m.group(1).strip() if m else ''


def parse_bpms(raw):
    """'0.000=155.120,32.000=160.0,...' → sorted [(beat, bpm)]"""
    out = []
    for pair in re.split(r'[,\n]+', raw):
        pair = pair.strip()
        if '=' in pair:
            b, v = pair.split('=', 1)
            try:
                out.append((float(b.strip()), float(v.strip())))
            except ValueError:
                pass
    out.sort()
    return out or [(0.0, 120.0)]


def parse_stops(raw):
    """'beat=dur_seconds,...' → sorted [(beat, dur_s)]"""
    out = []
    if not raw:
        return out
    for pair in re.split(r'[,\n]+', raw):
        pair = pair.strip()
        if '=' in pair:
            b, v = pair.split('=', 1)
            try:
                out.append((float(b.strip()), float(v.strip())))
            except ValueError:
                pass
    out.sort()
    return out


# ── timing engine ──────────────────────────────────────────────────────────────

def make_beat_fn(bpms, stops, offset_ms):
    """
    Returns  beat_fn(beat) → float ms from audio start.

    offset_ms  = #OFFSET value * 1000  (Stepmania convention: beat 0 is at
                 offset_ms ms from audio start; negative = chart starts early)
    """
    # Build a sorted list of (start_beat, start_time_from_beat0_ms, bpm_after)
    # "start_time_from_beat0_ms" is time elapsed since beat 0 at the START of this segment
    segments = []

    all_events = {}
    for beat, bpm in bpms:
        all_events.setdefault(beat, {})['bpm'] = bpm
    for beat, dur in stops:
        all_events.setdefault(beat, {})['stop'] = dur

    cur_beat = 0.0
    cur_t    = 0.0
    cur_bpm  = bpms[0][1]

    # Seed current BPM from beat-0 BPM entry if present
    if 0.0 in all_events and 'bpm' in all_events[0.0]:
        cur_bpm = all_events[0.0]['bpm']

    segments.append((cur_beat, cur_t, cur_bpm))

    for ev_beat in sorted(all_events.keys()):
        if ev_beat <= 0.0:
            continue
        ev = all_events[ev_beat]
        # advance time to this beat
        cur_t   += (ev_beat - cur_beat) * 60000.0 / cur_bpm
        cur_beat = ev_beat
        if 'bpm' in ev:
            cur_bpm = ev['bpm']
            segments.append((cur_beat, cur_t, cur_bpm))
        if 'stop' in ev:
            cur_t += ev['stop'] * 1000.0
            segments.append((cur_beat, cur_t, cur_bpm))

    def beat_fn(target_beat):
        # Binary search for the last segment whose start_beat <= target_beat
        lo, hi = 0, len(segments) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if segments[mid][0] <= target_beat:
                lo = mid
            else:
                hi = mid - 1
        sb, st, bpm = segments[lo]
        return offset_ms + st + (target_beat - sb) * 60000.0 / bpm

    return beat_fn


# ── note decoding ──────────────────────────────────────────────────────────────

TAP_CHARS = frozenset('124')   # tap(1), hold-head(2), roll-head(4); tail(3) is ignored


def row_to_bitmask(row, is_solo, is_double):
    """One text row → 4-bit column bitmask (bit0=L bit1=D bit2=U bit3=R)."""
    bm = 0
    row = row.strip()
    if is_solo and len(row) >= 6:
        # dance-solo layout: UL L D U R UR
        if row[0] in TAP_CHARS: bm |= 0x01   # UL → L
        if row[1] in TAP_CHARS: bm |= 0x01   # L  → L
        if row[2] in TAP_CHARS: bm |= 0x02   # D  → D
        if row[3] in TAP_CHARS: bm |= 0x04   # U  → U
        if row[4] in TAP_CHARS: bm |= 0x08   # R  → R
        if row[5] in TAP_CHARS: bm |= 0x08   # UR → R
    elif is_double and len(row) >= 8:
        # dance-double: LDUR | LDUR — fold both halves into one LDUR
        if row[0] in TAP_CHARS or row[4] in TAP_CHARS: bm |= 0x01
        if row[1] in TAP_CHARS or row[5] in TAP_CHARS: bm |= 0x02
        if row[2] in TAP_CHARS or row[6] in TAP_CHARS: bm |= 0x04
        if row[3] in TAP_CHARS or row[7] in TAP_CHARS: bm |= 0x08
    elif len(row) >= 4:
        # dance-single: L D U R
        if row[0] in TAP_CHARS: bm |= 0x01
        if row[1] in TAP_CHARS: bm |= 0x02
        if row[2] in TAP_CHARS: bm |= 0x04
        if row[3] in TAP_CHARS: bm |= 0x08
    return bm


def decode_measures(note_data, beat_fn, is_solo, is_double):
    """
    Parse raw Stepmania measure text (comma-separated) into
    sorted [(time_ms_int, bitmask)] with chords merged.
    Notes before audio start (time_ms < 0) are discarded.
    """
    note_data = re.sub(r'//[^\n]*', '', note_data)   # strip inline comments
    events    = {}   # time_ms_int → merged bitmask

    current_beat = 0.0
    for measure in note_data.split(','):
        rows = [l.strip() for l in measure.splitlines() if l.strip()]
        if not rows:
            current_beat += 4.0
            continue
        beat_step = 4.0 / len(rows)
        for i, row in enumerate(rows):
            bm = row_to_bitmask(row, is_solo, is_double)
            if bm:
                t_ms = beat_fn(current_beat + i * beat_step)
                if t_ms >= 0.0:
                    ti = int(round(t_ms))
                    events[ti] = events.get(ti, 0) | bm
        current_beat += 4.0

    return sorted(events.items())


# ── SM parser (classic .sm format) ────────────────────────────────────────────

def parse_sm(text, beat_fn):
    """Extract difficulties from a classic SM-format text."""
    diffs = []
    for block_m in re.finditer(r'#NOTES\s*:(.*?);', text, re.IGNORECASE | re.DOTALL):
        # SM fields: stepstype : desc : difficulty : meter : radar : <note data>
        parts = block_m.group(1).split(':', 5)
        if len(parts) < 6:
            continue
        stepstype = parts[0].strip().lower()
        if 'dance' not in stepstype:
            continue
        is_solo   = 'solo'   in stepstype
        is_double = 'double' in stepstype
        diff_name = parts[2].strip()[:8]
        try:
            meter = int(parts[3].strip())
        except ValueError:
            meter = 1
        notes = decode_measures(parts[5], beat_fn, is_solo, is_double)
        diffs.append({'name': diff_name, 'meter': min(meter, 255), 'notes': notes})
    return diffs


# ── SSC parser ─────────────────────────────────────────────────────────────────

def parse_ssc(text, beat_fn):
    """Extract difficulties from an SSC-format text (#NOTEDATA: blocks)."""
    diffs = []
    for block in re.split(r'#NOTEDATA\s*:', text, flags=re.IGNORECASE)[1:]:
        def gtag(name):
            m = re.search(r'#' + name + r'\s*:(.*?);', block, re.IGNORECASE | re.DOTALL)
            return m.group(1).strip() if m else ''
        stepstype = gtag('STEPSTYPE').lower()
        if 'dance' not in stepstype:
            continue
        is_solo   = 'solo'   in stepstype
        is_double = 'double' in stepstype
        diff_name = gtag('DIFFICULTY')[:8]
        try:
            meter = int(gtag('METER'))
        except ValueError:
            meter = 1
        notes_m = re.search(r'#NOTES\s*:(.*?)(?=#[A-Z_]|\Z)', block,
                             re.IGNORECASE | re.DOTALL)
        if not notes_m:
            continue
        raw = notes_m.group(1).rstrip('; \t\n')
        notes = decode_measures(raw, beat_fn, is_solo, is_double)
        diffs.append({'name': diff_name, 'meter': min(meter, 255), 'notes': notes})
    return diffs


# ── NNR encoder ────────────────────────────────────────────────────────────────

def encode_nnr(title, artist, bpm, offset_ms, diffs):
    nd = min(len(diffs), MAX_DIFFS)

    # Build per-diff byte streams
    streams = []
    for d in diffs[:nd]:
        buf   = bytearray()
        prev  = 0
        count = 0
        for ti, bm in d['notes']:
            delta = ti - prev
            if delta < 0:
                continue
            while delta > MAX_DELTA:
                buf   += struct.pack('<HB', MAX_DELTA, 0)
                count += 1
                delta -= MAX_DELTA
            buf   += struct.pack('<HB', delta, bm)
            count += 1
            prev = ti
        streams.append((bytes(buf), count))

    # Header (76 bytes)
    title_b  = title[:32].encode('utf-8', errors='replace').ljust(32, b'\x00')
    artist_b = artist[:32].encode('utf-8', errors='replace').ljust(32, b'\x00')
    header   = (MAGIC + title_b + artist_b
                + struct.pack('<f', float(bpm))
                + struct.pack('<hBB', max(-32768, min(32767, int(offset_ms))), nd, 0))
    assert len(header) == HEADER_SZ

    # Diff table (nd × 16 bytes)
    diff_table = bytearray()
    cur_off    = HEADER_SZ + nd * DIFF_SZ
    for i, d in enumerate(diffs[:nd]):
        name_b = d['name'][:8].encode('utf-8', errors='replace').ljust(8, b'\x00')
        diff_table += name_b + struct.pack('<BBHI', d['meter'], 0,
                                          streams[i][1], cur_off)
        cur_off += len(streams[i][0])
    assert len(diff_table) == nd * DIFF_SZ

    return header + bytes(diff_table) + b''.join(s[0] for s in streams)


# ── entry point ────────────────────────────────────────────────────────────────

def convert(input_path, output_path=None):
    if output_path is None:
        base        = os.path.splitext(input_path)[0]
        output_path = base + '.nnr'
        # Avoid clobbering the source if it's already named .nnr
        if output_path.lower() == input_path.lower():
            output_path = base + '_converted.nnr'

    with open(input_path, 'r', encoding='utf-8', errors='replace') as f:
        text = f.read()

    # Detect format
    is_ssc = bool(re.search(r'#NOTEDATA\s*:', text, re.IGNORECASE))

    title     = get_tag(text, 'TITLE')  or os.path.splitext(os.path.basename(input_path))[0]
    artist    = get_tag(text, 'ARTIST') or ''
    bpms      = parse_bpms(get_tag(text, 'BPMS') or get_tag(text, 'BPM'))
    stops     = parse_stops(get_tag(text, 'STOPS'))
    offset_s  = float(get_tag(text, 'OFFSET') or '0')
    offset_ms = offset_s * 1000.0
    bpm       = bpms[0][1]

    beat_fn = make_beat_fn(bpms, stops, offset_ms)
    diffs   = parse_ssc(text, beat_fn) if is_ssc else parse_sm(text, beat_fn)

    print(f"  Title  : {title}")
    print(f"  Artist : {artist}")
    print(f"  BPM    : {bpm}  |  Offset : {offset_ms:.0f} ms  |  Format: {'SSC' if is_ssc else 'SM'}")
    print(f"  Diffs  : {len(diffs)}")
    for d in diffs:
        print(f"    {d['name']:12s} meter={d['meter']:2d}  notes={len(d['notes'])}")

    if not diffs:
        print("ERROR: no playable dance difficulties found in this file.")
        return False

    data = encode_nnr(title, artist, bpm, offset_ms, diffs)

    with open(output_path, 'wb') as f:
        f.write(data)

    print(f"  Output : {output_path}  ({len(data):,} bytes)")
    return True


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    inp = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None
    ok  = convert(inp, out)
    sys.exit(0 if ok else 1)
