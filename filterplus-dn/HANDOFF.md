# Filter+ DN handoff

Digitone mk1/Keys, OS 1.44, core 4.0. Adapter id `filterplus-dn`, version 1.0.

## Integration

Six patch sites plus `ev_tick` / `ev_draw` / `ev_key` / `ev_enc`:

| site | address | stock | change |
|---|---|---|---|
| TYPE range | `0x4018db2c` | `00000300` | `00001000` (max 16) |
| TYPE formatter | `0x4018db54` | `40014b6c` | pointer to `digifilter_dn_typefmt` |
| per-voice filter | `0x4009e0d2` | `068080004110` | `jsr digifilter_dn_dispatch` |
| value-text max | `0x400885bc` | `7403` | `7410` (16) |
| value-text max | `0x400885c2` | `7003` | `7010` (16) |
| value-text table | `0x400885c4` | `41f9401a75e0` | `lea digifilter_dn_types` |

`digifilter_dn_dispatch` does the displaced `add.l #0x80004110,%d0`, stashes
voice/buffer/cutoff, reads TYPE from `fw_voice_params` slot 54, and points `a5`
at `digifilter_dn_filt` for TYPE 4..16 or at the stock `FILTER` target
otherwise.

## Runtime

`digifilter_dn_filt` (in `filter_ui.c`):

- skips all-zero (silent) voice blocks and resets state on the transition;
- reads TYPE/RESO/HARM/DAMP from `fw_voice_params`;
- uses the dispatcher's captured render cutoff `d7` so the filter envelope
  modulates every mode like the stock filter;
- TYPE 4..16:
  - BP/BP2 -> `df_svf_run`;
  - COMB/TRASH -> `df_comb_run`;
  - LP1/HP1/LP4-/HP4-/LP2/HP2/NOTCH/COMB-/TRASH- -> `df_lc_run`.

TYPE map: 4 BP, 5 BP2, 6 COMB, 7 TRASH, 8 LP1, 9 HP1, 10 LP4-, 11 HP4-,
12 LP2, 13 HP2, 14 NOTCH, 15 COMB-, 16 TRASH-.

## TYPE persistence

The stock temp save resets TYPE above 3. `dn_persist_type` mirrors TYPE 4..16
into the low byte of `BASE` (slot 62, a stock savable 8.8 param) and restores
it on the next UI tick after a load. HARM/DAMP use new params 182/183 (slots
65/26) and are best-effort across the stock pattern/temp format.

## UI

`filter_ui.c`:

- finds the FLTR pages, caches the FREQ/RES curve on `(mode, fi, qi)`;
- draws the curve and a 17x17 TYPE glyph;
- the glyph is a smooth response line with its pole order (1, 2 or 4) in the
  upper-left corner, except COMB/COMB-/TRASH/TRASH- which keep the teeth
  bitmap;
- maps page-2 C/G to HARM/DAMP for COMB/TRASH.

## CPU

The DSP kernels are shared with the Digitakt; see the root `README.md` CPU
table. The Digitone adapter overhead (cost guard, `fw_voice_params` reads and
dispatcher globals) is small but is not separately cycle-benched in 1.0.

## Verification

- `elekloader.sdk.build filterplus-dn --stock <Digitone_OS1.44.syx>`
- `elekloader.lint out/filterplus-dn-1.0.elemod --stock ... --with core-dn1-4.0-os1.44.elemod`
- `elekloader.patch --stock ... --mod core ... --mod out/filterplus-dn-1.0.elemod --check`
- `tests/filter_model.py`, `tests/comb_equivalence.py`, `tests/df_bench.py`
  cover the shared kernels.
- Hardware/emulator validation of the DN adapter's TYPE list, curve, glyphs,
  persistence and 8-voice cost is still pending.

## Known issues

- Assumes 48 kHz and savable slots 65/26/62 (the original DN port notes).
- HARM/DAMP are not guaranteed in the stock temp format; use project/pattern
  save when they matter.
- Hardware validation of Filter+ 1.0 is pending.
- `RE_NOTES_DN.md` is the original DN port RE history. OB4/OB2 sections are
  historical; OB is not compiled.

## Files

| file | contents |
|---|---|
| `mod.json` | id, version, 6 sites, TYPE 4..16, core 4.0 |
| `filter_glue.s` | formatter, names, value-text table, dispatcher |
| `filter_ui.c` | DN integration, curves/glyphs, page-2, persistence, events |
| `RE_NOTES_DN.md` | original DN RE notes (historical) |
