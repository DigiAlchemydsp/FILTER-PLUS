# Filter+ handoff

Status: **Filter+ 1.0**, built and verified for the Digitakt mk1 (OS 1.54) and
Digitone mk1/Keys (OS 1.44), core 4.0. The public repository is a fresh
single-commit project; the original DigiFilter history remains in the
DigiFilter repository.

- Repo: https://github.com/DigiAlchemydsp/FILTER-PLUS
- Release: https://github.com/DigiAlchemydsp/FILTER-PLUS/releases/tag/v1.0
- Local checkout: `C:\Users\benan\Music\ELEKTRON\FILTER-PLUS`

## What is in 1.0

13 filter types on each machine, all cheaper than the stock Elektron filter
types. Stock TYPE values still run the stock filter.

| machine | TYPE | modes |
|---|---|---|
| Digitakt | 8..20 | BP, BP2, COMB, TRASH, LP1, HP1, LP4-, HP4-, LP2, HP2, NOTCH, COMB-, TRASH- |
| Digitone | 4..16 | BP, BP2, COMB, TRASH, LP1, HP1, LP4-, HP4-, LP2, HP2, NOTCH, COMB-, TRASH- |

Family summary:

- **BP / BP2** - trapezoidal SVF band-pass, 2-pole.
- **COMB / TRASH** - full feedback combs with fractional delay and damping.
- **LP1 / HP1** - 1-pole TPT, 6 dB/oct.
- **LP4- / HP4-** - four cascaded 1-poles, 24 dB/oct, no resonance.
- **LP2 / HP2 / NOTCH** - 2-pole SVF mixes (biquad-equivalent).
- **COMB- / TRASH-** - integer-delay lofi combs, no fractional read or damping.

OB4/OB2 were removed from Filter+. The OB research files (`shared/ob1_*`,
`docs/ob1.md`, `tests/ob1_*`, `tools/gen_ob1_fbsat.py`) remain in the tree as
historical artifacts but are **not** compiled into either elemod and are
excluded from the release zip.

## Layout

| path | contents |
|---|---|
| `shared/` | firmware-free DSP: SVF, combs, 1-pole/4-pole, 2-pole mixes, tables |
| `filterplus-dt/` | Digitakt adapter, manifest, UI, DT notes |
| `filterplus-dn/` | Digitone adapter, manifest, UI, DN notes |
| `docs/` | design notes, reference mapping, type GIF |
| `tests/` | host and digiemu checks |
| `tools/` | table generators, release builder, UI/GIF previews |
| `releases/` | 1.0 elemods and `FilterPlus-1.0.zip` |

## Build and release

Requirements: m68k-elf toolchain, elekloader 0.5+, stock Digitakt 1.54 and
Digitone 1.44 images, and the matching core 4.0 elemods.

```sh
python tools/build_release.py \
    --elekloader <elekloader checkout> \
    --stock-dt <Digitakt_OS1.54.syx> \
    --stock-dn <Digitone_OS1.44.syx> \
    --toolchain-bin <m68k-elf bin>
```

The script builds both elemods, lints each against its core, patch-checks the
combined image, stages the elemods in `releases/elemods/`, writes local test
`.syx` files under `out/`, and creates `releases/FilterPlus-1.0.zip`.

## CPU

Cycles per voice per 32-sample block, measured on the Digitakt adapter with
`tests/df_bench.py` (digiemu cycle model). The DSP kernels are shared with the
Digitone. Stock reference: 2603 cycles/voice/block, measured on the stock DT
1.53 multimode filter (20,821 cycles per 8-voice block, ~2265 instructions per
voice). Per-TYPE stock costs were not separately benched.

| rank | TYPE | static | vs stock (static / swept) | swept |
|---:|---|---:|---:|---:|
| - | Stock multimode (LP2 / HP / LP4 / EQ) | 2603 | 100% / 100% | 2603 |
| 1 | LP1 | 606 | 23% / 23% | 593 |
| 2 | HP1 | 671 | 26% / 26% | 665 |
| 3 | TRASH- | 1035 | 40% / 40% | 1035 |
| 4 | COMB- | 1072 | 41% / 40% | 1044 |
| 5 | BP2 | 1318 | 51% / 88% | 2294 |
| 6 | BP | 1346 | 52% / 91% | 2363 |
| 7 | NOTCH | 1465 | 56% / 92% | 2403 |
| 8 | HP2 | 1480 | 57% / 93% | 2423 |
| 9 | LP2 | 1481 | 57% / 93% | 2421 |
| 10 | LP4- | 1562 | 60% / 60% | 1556 |
| 11 | HP4- | 1633 | 63% / 62% | 1626 |
| 12 | TRASH | 2023 | 78% / 78% | 2022 |
| 13 | COMB | 2037 | 78% / 78% | 2022 |

Worst-case (swept) order: LP1, HP1, TRASH-, COMB-, LP4-, HP4-, TRASH, COMB,
BP2, BP, NOTCH, LP2, HP2.

## Verification

- Both elemods build and link with core 4.0.
- `elekloader.lint` and `elekloader.patch --check` pass for both stock images.
- `tests/filter_model.py`: BP/LP/HP/NOTCH SVF mixes.
- `tests/comb_equivalence.py`: COMB/TRASH bit-exact against the baseline.
- `tests/df_bench.py`: all 13 types, including the output smoke check.
- Screenshots/previews: `tools/render_ui_previews.py` writes `shots/lowcpu/`;
  `tools/make_type_gif.py` writes the scrolling type GIF used in the README.

## Known issues and next steps

- Hardware validation of the 1.0 builds is pending.
- The CPU table is measured on the Digitakt adapter; the Digitone per-voice
  adapter overhead is not separately benched.
- Stock filter costs are an aggregate 1.53 reference; individual stock TYPE
  costs are not split.
- LP2/HP2/NOTCH use the shared SVF core rather than a separate DF2T biquad, to
  keep the coefficient tables compact.
- COMB-/TRASH- are intentionally lofi: integer delay, no damping/fractional
  read. They are the cheap comb options, not replacements for COMB/TRASH.
- The release zip is a 1.0 snapshot; rebuild it if the README or docs change
  after the release.
- Historical OB files/tests remain in the tree. Remove them in a future cleanup
  if the research history is no longer wanted.

## Related projects

- DigiFilter (original project/history): https://github.com/DigiAlchemydsp/DigiFilter
- Filter+FX: https://github.com/DigiAlchemydsp/Filter-FX
