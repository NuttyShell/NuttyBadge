# NutNutRevolution

A DDR-style rhythm game for NuttyBadge (ESP32-S3, 128×64 OLED).  
Four-lane falling arrow notes timed to MP3 audio.

---

## Controls

| Button | Action |
|--------|--------|
| LEFT   | Hit L lane |
| DOWN   | Hit D lane |
| UP     | Hit U lane |
| RIGHT  | Hit R lane |
| UP / DOWN (menu) | Navigate |
| A      | Confirm |
| SELECT (hold) | Quit game |

---

## Scoring

| Judgement | Window | Points |
|-----------|--------|--------|
| PERFECT   | ±45 ms | 300 |
| GOOD      | ±90 ms | 100 |
| MISS      | > 90 ms | 0, resets combo |

### RGB LED feedback
- **Blue** — PERFECT
- **Green** — GOOD
- **Red** — MISS

### Rank (end-of-song)
| Rank | Condition |
|------|-----------|
| S | All PERFECT, zero GOOD/MISS |
| A | No MISS |
| B | ≥ 90 % hit rate |
| C | ≥ 70 % hit rate |
| D | Below 70 % |

---

## Built-in songs

Both charts are stored in firmware flash. Audio is loaded from the SD card.

| Song | Artist | Audio path on SD |
|------|--------|-----------------|
| BUTTERFLY | SMILE.dk | `/sdcard/NutNutRevolution/songs/butterfly/BUTTERFLY.mp3` |
| NIGHT OF FIRE | MIKO | `/sdcard/NutNutRevolution/songs/night of fire/Night Of Fire.mp3` |

---

## Adding songs from SD card

1. Create a folder under `/sdcard/NutNutRevolution/songs/<songname>/`
2. Place one `.nnr` chart file and one `.mp3` audio file in it.
3. Insert the SD card and launch the game — the song appears in the song select menu automatically.

Example layout:
```
/sdcard/NutNutRevolution/songs/
    butterfly/
        BUTTERFLY.mp3
    night of fire/
        Night Of Fire.mp3
    freedomdive/
        FREEDOMDIVE.nnr
        FREEDOMDIVE.mp3
```

---

## Converting Stepmania charts

Use the included `ssc_to_nnr.py` converter to produce `.nnr` files from Stepmania `.sm` or `.ssc` charts.

```
python ssc_to_nnr.py SONGNAME.sm
python ssc_to_nnr.py SONGNAME.ssc
```

Output is written to `SONGNAME.nnr` next to the input file.  
If the input is already named `.nnr` (Stepmania file renamed), the output is named `SONGNAME_converted.nnr`.

Supported step types:
- `dance-single` (4 columns: L D U R)
- `dance-solo` (6 columns: UL/L merged → L, D, U, R/UR merged → R)
- `dance-double` (8 columns folded into 4)

BPM changes and stops are handled. Hold heads and roll heads are treated as taps; hold tails are ignored.

---

## NNR binary format

```
Header  76 B
  "NNR1"       4 B   magic
  title        32 B  null-padded UTF-8
  artist       32 B  null-padded UTF-8
  bpm          4 B   float32 little-endian
  offset_ms    2 B   int16 little-endian (Stepmania #OFFSET * 1000)
  num_diffs    1 B
  reserved     1 B

Diff entry  16 B  (repeated num_diffs times)
  name         8 B   null-padded UTF-8
  meter        1 B
  reserved     1 B
  note_count   2 B   uint16 little-endian
  data_offset  4 B   uint32 little-endian (byte offset from file start)

Note event  3 B  (repeated note_count times per diff)
  delta_ms     2 B   uint16 little-endian, ms since previous note (or since t=0)
  columns      1 B   bitmask: bit0=L  bit1=D  bit2=U  bit3=R
```

Note times are absolute milliseconds from audio start.  
If a gap between two notes exceeds 65535 ms, a silent spacer event (columns=0) is inserted.

---

## Audio sync tuning

The PWM ring buffer introduces a fixed latency between `audio_player_play()` and the first audible sample.  
This offset is defined in `NutNutRevolution.c`:

```c
#define NNR_AUDIO_LATENCY_MS  85
```

- Increase if notes appear **too early** relative to the beat.
- Decrease if notes appear **too late**.
