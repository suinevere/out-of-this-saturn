# Out of this World for Sega Saturn

A Sega Saturn port of Eric Chahi's *Out of this World*, based on Gregory Montoir's re-implementation and Fabien Sanglard's `raw` rewrite.

It builds as a standalone disc, and also ships as Part I of [heart-of-the-saturn](https://github.com/suinevere/heart-of-the-saturn).

## Playing

Download the setup kit from [Releases](https://github.com/suinevere/out-of-this-saturn/releases), unzip it, put your PC DOS data files in the data folder, and run `run-me.bat`.

The release also includes `0.bin`, the program linked at `0x06004000`, for embedding on another disc.

## Game data

No game data is included. You need the English PC DOS release: `bank01` through `bank0d` and `memlist.bin`.

## Requirements

- Git with SSH access to GitHub
- A POSIX shell or `cmd.exe`
- A host `g++` with C++11, for the tests

## Setup

```sh
git clone --recurse-submodules git@github.com:suinevere/out-of-this-saturn.git
cd out-of-this-saturn

cd SaturnRingLib && ./setup_compiler.bat && cd ..
cd tools/assets && ./data.bat /path/to/dos-files && cd ../..
```

`setup_compiler.bat` installs the SH-2 toolchain into `SaturnRingLib/Compiler`. `data.bat` installs the game data into `saturn/cd/data` from a folder, a `.zip` or a URL to a `.zip`; without it the build boots to a resource panic.

## Build

```sh
cd saturn
./compile.bat             # debug
./compile.bat release
./compile.bat clean
```

Output goes to `saturn/BuildDrop`. Point an emulator at the `.cue`.

When building from heart-of-the-saturn, `compile.bat` honours `SRL_INSTALL_ROOT` and `SRL_COMPILER_DIR` if set.

## Tests

```sh
cd saturn/tests
./run_tests.sh
```

A compile error aborts the script, so check for the `all suites passed` line rather than grepping for `FAIL`.

## Layout

```
saturn/src/       main.cxx and the engine, video, sound, menus, input, save, ports and system code
saturn/tests/     Host unit tests
saturn/cd/        Disc skeleton
saturn/BuildDrop/ Build output
SaturnRingLib/    SDK submodule and toolchain
tools/            Asset and analysis scripts
```

## License

GPL-2.0-or-later. See [LICENSE.md](LICENSE.md).

## Credits

Eric Chahi made the game. Gregory Montoir wrote the original re-implementation and Fabien Sanglard the rewrite this port started from. The port is built on ReyeMe's [SaturnRingLib](https://github.com/ReyeMe/SaturnRingLib).
