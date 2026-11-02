# Filter+ DN - Digitone mk1/Keys

Per-voice filter modes for the **Digitone mk1 / Keys, OS 1.44**, on top of the
stock OFF/LP2/HP/LP4 (TYPE 0..3). Filter+ adds 13 TYPE values, all cheaper than
the stock filter types, using each voice's own FREQ, RESO and filter envelope.
Requires core 4.0.

## Modes

| TYPE | mode | what it is |
|---|---|---|
| 4 | **BP** | state-variable band pass |
| 5 | **BP2** | wider band pass (about half the resonance) |
| 6 | **COMB** | feedback comb, delay from FREQ, feedback from RESO |
| 7 | **TRASH** | second comb at half the delay |
| 8 | **LP1** | 1-pole lowpass, 6 dB/oct |
| 9 | **HP1** | 1-pole highpass, 6 dB/oct |
| 10 | **LP4-** | four cascaded 1-poles, 24 dB/oct, no resonance |
| 11 | **HP4-** | four cascaded 1-pole highpasses, 24 dB/oct |
| 12 | **LP2** | 2-pole lowpass (SVF), 12 dB/oct with RESO |
| 13 | **HP2** | 2-pole highpass (SVF), 12 dB/oct with RESO |
| 14 | **NOTCH** | 2-pole notch (SVF) at FREQ |
| 15 | **COMB-** | integer-delay comb, lower CPU than COMB |
| 16 | **TRASH-** | integer-delay comb at half the delay, lower CPU than TRASH |

Stock TYPE 0..3 keeps running the stock multimode filter. The FLTR page redraws
the FREQ/RES response curve and the 17x17 TYPE-box icon; each icon is a smooth
response curve with its pole order (1, 2 or 4) in the upper-left corner.

## Controls

- **FREQ (E)** sets cutoff / comb pitch; **RESO (F)** is resonance and, for
  COMB/TRASH, the comb feedback (0..127). **TYPE (G)** selects the mode.
  The filter envelope (ENV + ADSR) modulates FREQ exactly like the stock types.
- **Second page**, while TYPE is COMB or TRASH, adds **C = Harmonics** (delay
  divider 1..4) and **G = Damping** (a one-pole on the feedback). The stock
  DEL/BASE/WIDTH controls keep working.

## Saving

The stock temp save (**FUNC+YES**) resets TYPE values above 3 on reload.
Filter+ mirrors TYPE 4..16 into the low byte of **BASE** (a stock savable 8.8
param) and restores it on the next UI tick after a load. HARM/DAMP are new
params (182/183) and remain best-effort across the stock pattern/temp format;
use **PTN > SAVE PATTERN TO** / **SAVE PATTERN KIT TO** (or **GLOBAL > PROJECT
> SAVE**) when you need them stored in a project.

## How it works

Six patch sites plus `ev_tick` / `ev_draw` / `ev_key` / `ev_enc`:

1. widen the Filter Type range field to max 16;
2. re-point the TYPE label formatter so 4..16 print the Filter+ names;
3. widen the value-text delegate to 16 and point it at a 17-entry table;
4. hook the per-voice filter call (TYPE 4..16) and run the shared Filter+ DSP;
5. add the HARM/DAMP params 182/183 and map page-2 C/G for COMB/TRASH;
6. draw the FLTR graph and TYPE-box icon from `ev_draw`.

## Build

```sh
export PATH="<m68k-elf bin>:$PATH"
export ELEKLOADER_CROSS=m68k-elf-
export PYTHONPATH="<elekloader checkout>"

python -m elekloader.sdk.build filterplus-dn --stock <Digitone_OS1.44.syx>
python -m elekloader.lint  out/filterplus-dn-1.0.elemod --stock <Digitone_OS1.44.syx> --with <core-dn1-4.0-os1.44.elemod>
python -m elekloader.patch --stock <Digitone_OS1.44.syx> --mod <core-dn1-4.0-os1.44.elemod> --mod out/filterplus-dn-1.0.elemod --out test.syx --version 1.0a --check
```

## Files

| file | contents |
|---|---|
| `mod.json` | id `filterplus-dn`, device `digitone-mk1`, OS 1.44, 6 sites, type 4..16, core 4.0 |
| `filter_glue.s` | TYPE names/list and per-voice dispatcher |
| `filter_ui.c` | DN integration, FLTR curves/icons, page-2 dials, tick/key/enc/draw, TYPE persistence |
| `../shared/df_dsp.c/h` | shared SVF/comb/1-pole/2-pole DSP |
| `../shared/filter_dsp.s` | ColdFire EMAC SVF and mix kernels |
| `../shared/filter_tables.h` | frequency, Q and comb-delay tables |
| `../shared/lc_tables.h` | 1-pole gain table |
| `../shared/svf_a1_tables.h` | precomputed SVF a1 coefficients |

## Testing

```sh
python tests/filter_model.py
python tests/comb_equivalence.py
python tests/df_bench.py       # needs m68k-elf-gcc + digiemu's patched Unicorn
```

## Releases

Prebuilt packages live in [`../releases/`](../releases/README.md). The current
package, **`FilterPlus-1.0.zip`**, carries one elemod per machine:
`elemods/filterplus-1.0.elemod` for the Digitakt mk1 (OS 1.54) and
`elemods/filterplus-dn-1.0.elemod` for the Digitone mk1/Keys (OS 1.44).

## Credits

- **Digital Alchemy DSP** - the Filter+ modes, DSP and FLTR-page drawing.
- **x2k_77-m1** - the original Digitone port and TYPE icons.

## Licence

GPL-2.0-or-later. No Elektron firmware is included or committed.
