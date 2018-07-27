# gba-tools

Collection of tools for Game Boy Advance development.

## gbafix

Pads and patches GBA ROMs.

Authors: [Dark Fader](https://github.com/darkfader), BlackThunder, [WinterMute](https://github.com/WinterMute), and [Diegoisawesome](https://github.com/Diegoisawesome).

Usage:
```
gbafix <romfile> [-p] [-t[title]] [-c<game_code>] [-m<maker_code>] [-r<version>] [-d<debug>]

    romfile         ROM input file
    -p              Pad to next exact power of 2. No minimum size!
    -t[<title>]     Patch title. Stripped filename if none given.
    -c<game_code>   Patch game code (four characters)
    -m<maker_code>  Patch maker code (two characters)
    -r<version>     Patch game version (number)
    -d<debug>       Enable debugging handler and set debug entry point (0 or 1)
```

## gbalzss

Compresses and uncompresses ROMs.

Author: [Michael Theall](https://github.com/mtheall)

Usage:
```
gbalzss [-h|--help] [--lz11] [--vram] <d|e> <infile> <outfile>

    -h, --help  Show this help
    --lz11      Compress using LZ11 instead of LZ10
    --vram      Generate VRAM-safe output (required by GBA BIOS)
    e               Compress <infile> into <outfile>
    d               Decompress <infile> into <outfile>
    <infile>        Input file (use - for stdin)
    <outfile>       Output file (use - for stdout)
```

## gbfs

Creates a GBFS archive.

Author: [Damian Yerrick](https://github.com/pinobatch)

Usage:
```
gbfs archive [file...]

    archive         Output file
    file            Input file(s)
```

## insgbfs

Inserts a GBFS file (or any other file) into a GBFS_SPACE (identified by symbol name) in a ROM.

Author: [Damian Yerrick](https://github.com/pinobatch)

Usage:
```
insgbfs sourcefile romfile symname

    sourcefile      Input file
    romfile         ROM file
    symname         symbol name
```

## lsgbfs

Lists objects in a GBFS file.

Author: [Damian Yerrick](https://github.com/pinobatch)

Usage
```
lsgbfs file

    file            Input GBFS file
```

## ungbfs

Dumps the objects in a GBFS file to separate files.

Author: [Damian Yerrick](https://github.com/pinobatch)

Usage:
```
ungbfs file

    file            Input GBFS file
```
