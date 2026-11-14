# Changes in vscp-cli

Every change that goes into a release is listed here, newest release first.
Add each change under **Unreleased** when it is made. When a release is
tagged, rename **Unreleased** to the version and date, and start a new
**Unreleased** section above it.

## Unreleased

- New command line tools, replacing the old vscpcmd code:
  - `vscp-send`: send events given on the command line, as VSCP string/JSON/XML, or from a file.
  - `vscp-dump`: show received events as text, VSCP string, JSON, XML or CSV, with time stamps, filters and a log file.
  - `vscp-gen`: generate random, fixed or counting events (protocol classes are left out unless allowed).
  - `vscp-reg`: read, write and dump node registers; read only registers are reported.
  - `vscp-info`: GUID, MDF URL, firmware version and the other standard registers; download and show the MDF.
  - `vscp-scan`: find nodes with "who is there" or a register read scan.
- All tools work over tcpip, udp, multicast, ws1, ws2, mqtt, socketcan, canal and level2, with TLS, AES-128/192/256 encryption and HMAC-SHA256 authentication where the transport has them. Passwords and keys can come from the environment.
- Builds and tests on Linux x86_64, Raspberry Pi arm64 and armhf, macOS arm64 and Windows x64 in CI, with downloadable artifacts, SHA256SUMS and build badges.
- Documentation in `docs/` (docsify) and example configuration files in `examples/`.
- Version year.month.patch in `src/vscp-cli-version.h`, updated on every push to main.
- Set up structure (dcd2db4)
- Moved vscpcmd to this project from the main project (d70acef)
- Initial commit (ed62db0)

No release has been tagged yet.
