# Out of this World for Sega Saturn

The disc image included here has the game program but none of the game data. You
provide that from your own copy. `run-me.bat` reads your files, adds them to the
disc image, and writes a `.cue` and `.bin` you can burn or mount.

## What you need

The game data comes from the PC DOS release. Put these fourteen files in
`(put bank and memlist files here)`:

```
bank01  bank02  bank03  bank04  bank05  bank06  bank07
bank08  bank09  bank0a  bank0b  bank0c  bank0d  memlist.bin
```

Upper or lower case both work, and a `.zip` is fine. Nothing else from that
release is used.

## Running it

Unzip somewhere with about 10 MB free, put your files in the folder, then:

- Windows: double-click `run-me.bat`
- Linux and macOS: `bash run-me.bat`

It writes the finished disc to:

```
Out of this World (USA)/
    Out of this World (USA).cue     <- burn or mount this
    Out of this World (USA).bin
```

## Requirements

Windows needs nothing else installed.

Linux and macOS need `xorriso`. If it is missing, `run-me.bat` says so, shows
the exact install command for your system, and asks whether to run it. Answer
`n` and it stops without installing anything, so you can do it yourself.

Do not unzip this kit to a path containing an apostrophe. The bundled Windows
`xorriso` is a Cygwin build and mishandles them.

## What's in here

| Path | |
|---|---|
| `run-me.bat` | The only thing you run |
| `(put bank and memlist files here)/` | Your PC DOS data files go here |
| `bin/` | The disc image before your data is added, the disc-building scripts, and bundled tools. See `bin/README.md` for licenses |
| `Out of this World (USA)/` | Written by `run-me.bat`: the finished disc |

## Acknowledgements

**hkzlab** — for the original idea.

**Gregory Montoir** — for the engine reimplementation this port is built on, and
**Fabien Sanglard** for the cleanup and the write-ups.

**ReyeMe** — for SaturnRingLib, which this is built against.

**The SegaXtreme forums** — for the Saturn hardware knowledge that made it
possible.

Out of this World was created by Eric Chahi. The game's data is not included here.
