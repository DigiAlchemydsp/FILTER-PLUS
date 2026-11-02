# Filter+ - Digitakt mk1

Per-track filter modes for the **Digitakt mk1, OS 1.54**, on top of the stock
OFF/LP/HP/EQ. Filter+ adds 13 TYPE values, all cheaper than the stock
Elektron filter types. It uses each track's own FREQ, RESO and filter
envelope, and requires core 4.0.

## Modes

| TYPE | mode | what it is |
|---|---|---|
| 8 | **BP** | state-variable band pass |
| 9 | **BP2** | wider band pass (about half the resonance) |
| 10 | **COMB** | feedback comb, delay from FREQ, feedback from RESO |
| 11 | **TRASH** | second comb at half the delay |
| 12 | **LP1** | 1-pole lowpass, 6 dB/oct |
| 13 | **HP1** | 1-pole highpass, 6 dB/oct |
| 14 | **LP4-** | four cascaded 1-poles, 24 dB/oct, no resonance |
| 15 | **HP4-** | four cascaded 1-pole highpasses, 24 dB/oct |
| 16 | **LP2** | 2-pole lowpass (SVF), 12 dB/oct with RESO |
| 17 | **HP2** | 2-pole highpass (SVF), 12 dB/oct with RESO |
| 18 | **NOTCH** | 2-pole notch (SVF) at FREQ |
| 19 | **COMB-** | integer-delay comb, lower CPU than COMB |
| 20 | **TRASH-** | integer-delay comb at half the delay, lower CPU than TRASH |

Stock TYPE values 0..7 keep running the stock filter. The FLTR page redraws
the FREQ/RES response curve and the 17x17 TYPE-box icon; each icon is a smooth
response curve with its pole order (1, 2 or 4) in the upper-left corner.

## Controls

- **FREQ (E)** sets cutoff / comb pitch; **RESO / GAIN (F)** is resonance and,
  for COMB/TRASH, the comb feedback (0..127). **TYPE (G)** selects the mode.
  The filter envelope (ENV + ADSR) modulates FREQ exactly like the stock types.
- **Second FLTR page**, while TYPE is COMB or TRASH, adds **C = Harmonics**
  (delay divider 1..4) and **G = Damping** (a one-pole on the feedback), shown
  as round dials. The stock Base/Width/Env Delay/SRR knobs keep working.

## How it works

Three patch sites plus `ev_tick` / `ev_draw`:

1. widen the Filter Type range field to max 20;
2. re-point the TYPE label formatter so 8..20 print the Filter+ names;
3. hook the per-voice filter call and run the shared Filter+ DSP for TYPE 8..20,
   tail-calling the stock filter for everything else;
4. draw the FLTR response curve and TYPE-box icon from `ev_draw`;
5. on each UI tick, map the second FLTR page's free C/G encoders when the
   active track is COMB/TRASH.

## CPU

Measured cycles per voice per 32-sample block (`tests/df_bench.py`, digiemu
cycle model). Stock reference: ~2603 cycles.

| mode | static | swept |
|---|---:|---:|
| LP1 | 606 | 593 |
| HP1 | 671 | 665 |
| TRASH- | 1035 | 1035 |
| COMB- | 1072 | 1044 |
| BP2 | 1318 | 2294 |
| BP | 1346 | 2363 |
| NOTCH | 1465 | 2403 |
| HP2 | 1480 | 2423 |
| LP2 | 1481 | 2421 |
| LP4- | 1562 | 1556 |
| HP4- | 1633 | 1626 |
| TRASH | 2023 | 2022 |
| COMB | 2037 | 2022 |

## Build

```sh
export PATH="<m68k-elf bin>:$PATH"
export ELEKLOADER_CROSS=m68k-elf-
export PYTHONPATH="<elekloader checkout>"

python -m elekloader.sdk.build filterplus-dt --stock <Digitakt_OS1.54.syx>
python -m elekloader.lint  out/filterplus-1.0.elemod --stock <Digitakt_OS1.54.syx> --with <core-4.0-os1.54.elemod>
python -m elekloader.patch --stock <Digitakt_OS1.54.syx> --mod <core-4.0-os1.54.elemod> --mod out/filterplus-1.0.elemod --out custom.syx --version 1.0a --check
```

## Files

| file | contents |
|---|---|
| `mod.json` | id `filterplus`, device `digitakt-mk1`, OS 1.54, 3 sites, type 8..20, core 4.0 |
| `filter.c` | Digitakt adapter: kit slots, envelope, dispatch |
| `filter_glue.s` | TYPE formatter and per-voice dispatcher |
| `filter_ui.c` | FLTR curve, TYPE icons, page-2 dials, tick/draw |
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
