# skylanders-portal

Skylanders Portal of Power emulation for [ReXGlue](https://github.com/rexglue/rexglue-sdk)
static recompilations of the Xbox 360 Skylanders games. It started as the portal code of
[Giants Recompiled](https://github.com/TheBiemGamer/GiantsRecomp) and is used as a git submodule by
[Trap Team Recompiled](https://github.com/TheBiemGamer/TrapTeamRecomp).

## Targets

- **`skylanders_portal_core`** has no ReXGlue dependency and is unit-tested. It holds the portal
  protocol (a software portal with up to 16 figures, including Trap Team traps), the Xbox 360
  frame format, figure files, the figure catalog, and figure save-data decoding.
- **`skylanders_portal_rex`** is the ReXGlue glue: settings (`portal_mode`, ...), the handler for
  the SDK's `XamInputNonControllerGetRaw/SetRaw` hook, a real portal over USB (hidapi), the F6
  figure overlay, and portal speaker audio. It is built only when the including project provides
  the `rex::runtime` target.

`figure_stats` uses the AES and MD5 implementations the ReXGlue SDK vendors, so set `REXSDK_DIR` to
an SDK source tree when configuring. Standalone test build:

```
cmake -S . -B out -G Ninja -DREXSDK_DIR=<path to rexglue-sdk>
cmake --build out && ctest --test-dir out
```

## Credits

- [Cemu](https://github.com/cemu-project/Cemu), [RPCS3](https://github.com/RPCS3/rpcs3) and
  [Dolphin](https://github.com/dolphin-emu/dolphin) already emulate the Portal of Power; their code
  was read to learn the commands, the Xbox 360 message format, the figure layout and the Trap Team
  speaker format. No code was copied.
- [Xenia Canary](https://github.com/xenia-canary/xenia-canary) for the XAM non-controller API.
- [Skylanders Ultimate NFC Pack](https://skylandersnfc.github.io/Skylanders-Ultimate-NFC-Pack/)
  for the figure dumps behind the name/id catalog.
- [SkyReader](https://github.com/reedstrm/SkyReader) and Marijn Kneppers'
  ["Reverse engineering Skylanders' Toys-to-life mechanics"](https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/)
  for figure save-data decoding.

## License

MIT (see `LICENSE`). Skylanders is a trademark of its owners; this project is unofficial.
