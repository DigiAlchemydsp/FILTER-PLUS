# Changelog

All notable changes to Filter+. Format-2
[elekloader](https://github.com/irpina/elekloader) mods for the Elektron
**Digitakt mk1 (OS 1.54)** and **Digitone mk1/Keys (OS 1.44)**.

## [1.0] - 2026-11-02

First Filter+ release. 13 filter types for both machines, all cheaper than the
stock Elektron filter types.

### Added

- **Digitakt mk1**: BP, BP2, COMB, TRASH, LP1, HP1, LP4-, HP4-, LP2, HP2,
  NOTCH, COMB-, TRASH- as TYPE 8..20.
- **Digitone mk1/Keys**: the same 13 modes as TYPE 4..16, including per-voice
  FREQ/RESO and filter-envelope modulation.
- **Low-CPU bank**:
  - LP1/HP1: 1-pole TPT, 6 dB/oct.
  - LP4-/HP4-: four cascaded 1-poles, 24 dB/oct, no resonance.
  - LP2/HP2/NOTCH: 2-pole SVF mixes.
  - COMB-/TRASH-: integer-delay combs with no fractional read or damping.
- **Smooth TYPE glyphs**: response-curve icons with the pole order (1/2/4) in
  the upper-left corner.
- **FLTR-page curves** for every mode, with peak/corner smoothing.
- **HARM/DAMP** on the second FLTR page for COMB/TRASH on both machines.
- **DN TYPE persistence**: TYPE 4..16 is mirrored into BASE and restored after
  a load.

### Removed

- OB4/OB2 are not part of Filter+. The remaining modes are all below the
  stock filter's CPU cost.

### Releases

- `FilterPlus-1.0.zip` carries `elemods/filterplus-1.0.elemod` (Digitakt 1.54)
  and `elemods/filterplus-dn-1.0.elemod` (Digitone 1.44).
