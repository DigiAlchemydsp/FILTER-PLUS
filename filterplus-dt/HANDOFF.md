# Filter+ DT handoff

Digitakt mk1, OS 1.54, core 4.0. Adapter id `filterplus`, version 1.0.

## Integration

Three patch sites plus `ev_tick` / `ev_draw`:

| site | address | stock | change |
|---|---|---|---|
| TYPE range | `0x401aa75c` | `00000700` | `00001400` (max 20) |
| TYPE formatter | `0x40153bda` | `40065336` | pointer to `digifilter_typefmt` |
| per-voice filter | `0x400780ba` | `068080001a18` | `jsr digifilter_dispatch` |

`digifilter_dispatch` (in `filter_glue.s`) does the displaced
`add.l #0x80001a18,%d0`, reads the voice TYPE, and points `a3` at
`digifilter_filt` for TYPE 8..20 or at the original stock/digihealth target
otherwise.

## Runtime

`digifilter_filt` (in `filter.c`) receives `(params, buf, active, voice)`:

- TYPE is `params[0]`.
- FREQ is `params@2` (index << 8) plus the filter-envelope term from
  `params@6` and the engine's per-voice envelope level.
- RESO is the engine slot `0x1b`, read at `0x80001502 + 106*voice + 0x36`.
- COMB/TRASH additionally read HARM from kit slot `0x2f` and DAMP from
  `0x30`.
- TYPE 8..20 dispatch:
  - BP/BP2 -> `df_svf_run` (BP/BP2 mixes).
  - COMB/TRASH -> `df_comb_run`.
  - LP1/HP1/LP4-/HP4-/LP2/HP2/NOTCH/COMB-/TRASH- -> `df_lc_run`.
  - Anything else tail-calls the stock filter at `0x40072844`.

Kit layout used by the UI: sound block `kit + 0x20 + t*0xa2`, slot `s` at
`+0x14 + 2*s`. TYPE 8..20 maps to modes 6..14 in the local enum.

## UI

`filter_ui.c`:

- caches the FREQ/RES response curve on `(mode, fi, qi)`;
- draws the curve in the FLTR box and a 17x17 TYPE glyph;
- the glyph is a smooth response line with its pole order (1, 2 or 4) in the
  upper-left corner, except COMB/COMB-/TRASH/TRASH- which keep the teeth
  bitmap;
- on the second FLTR page, COMB/TRASH map the free C/G encoders to HARM/DAMP.

## CPU

See the root `README.md`/`HANDOFF.md` table. DT adapter measurements:

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

Stock reference: 2603 cycles/voice/block (DT 1.53 stock multimode).

## Verification

- `elekloader.sdk.build filterplus-dt --stock <Digitakt_OS1.54.syx>`
- `elekloader.lint out/filterplus-1.0.elemod --stock ... --with core-4.0-os1.54.elemod`
- `elekloader.patch --stock ... --mod core ... --mod out/filterplus-1.0.elemod --check`
- `tests/filter_model.py`
- `tests/comb_equivalence.py`
- `tests/df_bench.py` (all 13 types; needs digiemu's patched Unicorn)
- Digiemu UI/audio scripts under `tests/digiemu_filter_*` are available for
  live checks; they use the DT 1.53 firmware folder and the adapter's patched
  test image.

## Known issues

- Hardware validation of Filter+ 1.0 is pending.
- `commit()`/`digifilter_voice` is the legacy coefficient-set path; the live
  audio path is `digifilter_filt`. It is harmless but could be removed.
- The old `RE_NOTES.md` is DigiFilter/DN-port reverse-engineering history. OB
  sections are historical; OB is not compiled.
- `HANDOFF-OPTIMIZATION.md` (old DigiFilter 1.1a performance ledger) is not
  included in Filter+; use `git log` in the DigiFilter repo for that history.

## Files

| file | contents |
|---|---|
| `mod.json` | id, version, 3 sites, TYPE 8..20, core 4.0 |
| `filter.c` | kit slots, envelope, per-voice dispatch |
| `filter_glue.s` | formatter, names, dispatcher |
| `filter_ui.c` | curves, glyphs, page-2 dials, tick/draw |
| `RE_NOTES.md` | original RE notes (historical) |
