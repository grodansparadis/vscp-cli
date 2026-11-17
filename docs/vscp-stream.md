# vscp-stream

Collect CLASS1.INFORMATION (20), type 36 (0x24), Stream Data. Display each
assembled stream as text (default) or a hexadecimal byte dump.

```
vscp-stream [OPTIONS]
```

## Assembly

Byte 0 is the sequence counter; bytes 1-7 are data. Collection starts at
sequence 0. Frames must arrive in order, including the wrap from 255 to 0.
A zero before the expected wrap starts a new stream and displays the old one.
Streams from different full GUIDs are kept separate.

There is **no standard end marker**. A stream is considered ready after
500 ms without another fragment from that GUID, adjustable with
`--ready-timeout`. A short frame, newline or NUL does not end a stream.
The idle boundary is a framing heuristic: lost final frames cannot be detected,
and a pause longer than the timeout splits collection. Choose a timeout longer
than the sender's inter-frame pauses.

Text is quoted, with C-style escapes for newlines, NUL, quotes, backslashes
and non-ASCII/control bytes. Hexadecimal output contains payload bytes only,
not sequence counters. Output includes local display time, source GUID and
payload byte count. Only native Level I events (at most eight data bytes)
are collected, not encapsulated class 532 or zoned stream type 38.

## Options

| Option | Description |
| :----- | :---------- |
| `-f, --format FMT` | `text` (default), `hex` or `binary`. `binary` means a hexadecimal dump, not raw stdout bytes |
| `--from GUID` | Exact full-GUID selection; all [supported GUID forms](formats.md) accepted, including `-` for zero |
| `-N, --node NICKNAME` | Match the last GUID byte, decimal or `0x` hexadecimal, 0-255 |
| `--ready-timeout MS` | Per-stream inactivity before display, default 500, minimum 1 |
| `--max-size BYTES` | Maximum total buffered payload across all streams, default 1048576 (1 MiB), minimum 1 |
| `-l, --log FILE` | Also append displayed records to FILE |
| `-q, --quiet` | No stdout output |
| `-n, --count N` | Exit after N assembled streams, not N frames; 0 means unlimited |
| `--idle MS` | Exit after MS without selected events; 0 (default) means unlimited |
| `--duration MS` | Exit after MS; 0 (default) means unlimited |

Plus all [connection and security options](connecting.md). `--binary` is
the shared TCP/IP wire-protocol option; use `-f binary` for hexadecimal output.

Without a selector, collect all nodes. If both selectors are supplied, both
must match. Nicknames are not globally unique; prefer `--from` across multiple
interfaces. No more than 1024 unfinished streams are buffered.

## Examples

```bash
vscp-stream -i socketcan --device can0 -N 7 -f hex
vscp-stream -i tcpip -H server --from '{::01,07}' -f text --ready-timeout 1000
vscp-stream -i udp --from -:07 -n 1 --duration 10000 -l stream.log
```

## Exit status

0 on normal completion or Ctrl+C; 1 for malformed payloads, missing,
duplicate or out-of-order nonzero fragments, buffer limits or output errors;
2 for usage/settings errors; 3 for connection failure/loss.
4 if a requested count is unmet, or unfinished data remains when
`--idle`/`--duration` expires. Set these stop timers longer than
`--ready-timeout`. Unfinished data is discarded with a stderr notification,
never printed as a ready stream. Start the receiver before the stream begins.

---

[filename](./bottom_copyright.md ':include')
