# vscp-cli

![License](https://img.shields.io/badge/license-MIT-blue.svg)
[![Linux](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-linux.yml/badge.svg?branch=main)](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-linux.yml?query=branch%3Amain+is%3Asuccess)
[![Raspberry Pi arm64](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-raspberry-pi-arm64.yml/badge.svg?branch=main)](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-raspberry-pi-arm64.yml?query=branch%3Amain+is%3Asuccess)
[![Raspberry Pi armhf](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-raspberry-pi-armhf.yml/badge.svg?branch=main)](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-raspberry-pi-armhf.yml?query=branch%3Amain+is%3Asuccess)
[![macOS](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-macos.yml/badge.svg?branch=main)](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-macos.yml?query=branch%3Amain+is%3Asuccess)
[![Windows](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-windows.yml/badge.svg?branch=main)](https://github.com/grodansparadis/vscp-cli/actions/workflows/build-windows.yml?query=branch%3Amain+is%3Asuccess)
[![Project Status: Active](https://www.repostatus.org/badges/latest/active.svg)](https://www.repostatus.org/#active)

Command line tools for [VSCP](https://www.vscp.org). Send, dump and generate
events, read and write registers, fetch the GUID and MDF of nodes and find
the nodes on a segment, over every VSCP transport: tcp/ip, UDP, multicast,
websockets (ws1/ws2), MQTT, SocketCAN, CANAL drivers and Level II drivers.
TLS, AES encryption and HMAC authentication are supported where the
transport has them. Runs on Linux (also Raspberry Pi 32/64-bit), macOS and
Windows.

| Tool | What it does |
| :--- | :----------- |
| [vscp-send](docs/vscp-send.md) | Send events, from the command line or replayed from a file |
| [vscp-dump](docs/vscp-dump.md) | Show received events as text, VSCP string, JSON, XML or CSV |
| [vscp-gen](docs/vscp-gen.md) | Generate random, fixed or counting events for testing |
| [vscp-reg](docs/vscp-reg.md) | Read, write and dump the registers of a node |
| [vscp-info](docs/vscp-info.md) | GUID, MDF URL, firmware version etc. of a node; download and show its MDF |
| [vscp-scan](docs/vscp-scan.md) | Find the nodes on a segment |

## Examples

```bash
vscp-dump -i socketcan --device can0              # everything on the bus
vscp-send -i socketcan --device can0 30 5 0 1 2   # CLASS1.CONTROL TurnOn, zone 1, subzone 2
vscp-info -i tcpip -H server -N 7 --mdf           # node 7 and its MDF
```

## Documentation

The [docs](docs/README.md) folder is a docsify site: getting started,
[install and download](docs/installing.md),
[connecting and security](docs/connecting.md) (all interfaces, TLS,
encryption, configuration files), [event formats](docs/formats.md) and one
page per tool. Every tool also has `--help`.

## Download

Every push builds ready to run programs for Linux x86_64, Raspberry Pi
(32/64-bit), macOS (Apple silicon) and Windows x64. Click a build badge above
to download an artifact from a run page, or fetch and verify the newest
builds of all platforms with
[scripts/download-artifacts.sh](scripts/download-artifacts.sh). See
[Install](docs/installing.md), which also covers
[building from source](docs/installing.md#building-from-source).

## License

MIT, see [LICENSE](LICENSE). Changes are listed in
[README_CHANGES.md](README_CHANGES.md).
