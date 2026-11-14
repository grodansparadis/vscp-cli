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
vscp-dump -i socketcan --device can0                       # everything on the bus
vscp-send -i socketcan --device can0 30 5 0 1 2            # CLASS1.CONTROL TurnOn, zone 1, subzone 2
vscp-scan -i udp --info                                    # which nodes are there?
vscp-info -i tcpip -H server -N 7 --mdf                    # node 7 and its MDF
vscp-reg  -i tcpip -H server -N 7 write 0x10 0x55          # write a register
vscp-gen  -i multicast -g 10 -n 1000                       # 1000 random events

# TLS + AES-256/HMAC to a VSCP daemon, keys from the environment
export VSCP_PASSWORD=secret VSCP_KEY=... VSCP_HMAC_KEY=...
vscp-dump -i tcpip -H vscp.example.com --tls --ca-file ca.crt -e aes256 -f json

# Encrypted UDP, plain datagrams dropped
vscp-dump -i udp -e aes128 --require-encryption
```

## Interfaces and security

| `-i` | Transport | Security |
| :--- | :-------- | :------- |
| `tcpip` | VSCP tcp/ip link protocol, text or binary mode | TLS; AES-128/192/256 + HMAC-SHA256 in binary mode |
| `udp` | VSCP over UDP | AES-128/192/256 + HMAC-SHA256 |
| `multicast` | VSCP multicast | AES-128/192/256 + HMAC-SHA256 |
| `ws1`, `ws2` | VSCP websocket protocols | TLS (wss); AES login authentication |
| `mqtt` | VSCP over MQTT | TLS, client certificates |
| `socketcan` | SocketCAN (Linux) | |
| `canal` | CANAL Level I driver | |
| `level2` | VSCP Level II driver | |

See [Connecting and security](docs/connecting.md) for all options, JSON
configuration files and `--print-config`.

## Download

Every push builds the tools for each platform. Click a badge above to see the
successful runs and download the artifact from a run page (GitHub only
serves artifacts to signed in users):

| Artifact | Platform |
| :------- | :------- |
| `vscp-cli-linux-x86_64` | Linux x86_64 (Debian 12, Ubuntu 22.04 and newer) |
| `vscp-cli-linux-arm64` | Raspberry Pi OS 64-bit |
| `vscp-cli-linux-armhf` | Raspberry Pi OS 32-bit |
| `vscp-cli-macos-arm64` | macOS, Apple silicon |
| `vscp-cli-windows-x64` | Windows x64 |

Each artifact holds a `SHA256SUMS-<artifact>` file. Linux needs
`libmosquitto1 libexpat1 libcurl4 libssl3`, macOS `brew install mosquitto
openssl@3 expat`; the Windows zip has all DLLs. See [Install](docs/installing.md).

## Build from source

```bash
sudo apt install build-essential cmake git libmosquitto-dev libexpat1-dev libssl-dev libcurl4-openssl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

macOS and Windows: see [Install](docs/installing.md#building-from-source).

## Documentation

The [docs](docs/README.md) folder is a docsify site: overview, install,
connecting and security, event formats and one page per tool. Every tool also
has `--help`.

## License

MIT, see [LICENSE](LICENSE). Changes are listed in
[README_CHANGES.md](README_CHANGES.md).
