# releases

Prebuilt Filter+ for the **Digitakt mk1 (OS 1.54)** and **Digitone mk1 /
Keys (OS 1.44)**. Both machines use the same 13 filter types, all cheaper than
the stock Elektron filter types. Latest OS only; one elemod per machine.

Published releases: [GitHub Releases](https://github.com/DigiAlchemydsp/FILTER-PLUS/releases)
(`v1.0` is current / Latest).

| file | |
|---|---|
| `elemods/filterplus-1.0.elemod` | the Digitakt mk1 mod, **OS 1.54** |
| `elemods/filterplus-dn-1.0.elemod` | the Digitone mk1/Keys mod, **OS 1.44** |
| `FilterPlus-1.0.zip` | the release package: both elemods + sources + README + LICENSE |

Each elemod needs its matching core and stock OS file:

| machine | elemod | core | stock |
|---|---|---|---|
| Digitakt mk1 | `filterplus-1.0.elemod` | `core-4.0-os1.54.elemod` | `Digitakt_OS1.54.syx` |
| Digitone mk1/Keys | `filterplus-dn-1.0.elemod` | `core-dn1-4.0-os1.44.elemod` | `Digitone_OS1.44.syx` |

Install (repeat with the other machine's files):

```sh
python -m elekloader.patch --stock Digitakt_OS1.54.syx \
    --mod core-4.0-os1.54.elemod \
    --mod releases/elemods/filterplus-1.0.elemod \
    --out Digitakt_OS1.54-filterplus.syx --version 1.0

python -m elekloader.patch --stock Digitone_OS1.44.syx \
    --mod core-dn1-4.0-os1.44.elemod \
    --mod releases/elemods/filterplus-dn-1.0.elemod \
    --out Digitone_OS1.44-filterplus.syx --version 1.0
```

Then flash the `.syx` with Elektron Transfer.

The package contains the mods' own bytes only; no Elektron firmware. See
[../LICENSE](../LICENSE). Rebuild with `tools/build_release.py` (see
[../README.md](../README.md)).
