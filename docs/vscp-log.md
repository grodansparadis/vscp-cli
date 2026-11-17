# vscp-log

Collect CLASS1.LOG (509) events, assembling readable messages by source GUID
and log channel.

```
vscp-log [OPTIONS]
```

## Messages

Type 1 contains channel ID, log level and zero-based fragment index in
bytes 0-2, followed by up to five message bytes. Fragments must begin at zero
and arrive consecutively with the same level. A new zero starts the next
message; unlike stream sequence counters, log indices do not wrap within a
message.

A message is displayed after 500 ms without another fragment for that
GUID/channel, on the next index zero, or on a channel start/stop event.
There is no standard per-message end marker, so inactivity is a framing
heuristic, not a guarantee that no final frames were lost. Choose
`--ready-timeout` longer than the sender's inter-fragment pauses.
Short frames, newline and NUL do not terminate messages.

Output includes local display time, full GUID, channel ID, numeric level,
level name, byte count and quoted message text. Levels 0-8 are named
Emergency, Alert, Critical, Error, Warning, Notice, Informational, Debug and
Verbose. Other levels are marked `Application-defined`.
Control/non-ASCII bytes, quotes and backslashes are escaped.

General, start, stop and level events are also displayed by name with
their payload in hexadecimal. Level-event data is preserved without assuming
an application-specific level-setting layout. Unknown event types remain
visible with their numeric type and raw payload.
Only native Level I log events with at most eight data bytes are collected.

## Options

| Option | Description |
| :----- | :---------- |
| `--from GUID` | Only this exact full GUID; all supported VSCP GUID forms accepted |
| `-N, --node NICKNAME` | Only this last GUID byte, 0-255, decimal or `0x` hexadecimal |
| `--ready-timeout MS` | Per-message idle boundary, default 500, minimum 1 |
| `--max-size BYTES` | Total buffered payload across all GUIDs/channels, default 1048576 (1 MiB), minimum 1 |
| `-l, --log FILE` | Also append displayed records to FILE |
| `-q, --quiet` | No stdout output |
| `-n, --count N` | Exit after N output records, including control events, not fragments; 0 means unlimited |
| `--idle MS` | Exit after MS without selected events; 0 (default) means unlimited |
| `--duration MS` | Exit after MS; 0 (default) means unlimited |

Plus all [connection and security options](connecting.md). No selector means
all nodes; both selectors together must match. Nicknames are not globally
unique; prefer `--from` across interfaces. Zero GUID selection is exact.
At most 1024 unfinished messages are held at once.

## Examples

```bash
vscp-log -i socketcan --device can0 -N 7
vscp-log -i tcpip -H server --from '{::01,07}' -l node7.log
vscp-log -i udp --ready-timeout 1000 --duration 60000 -q -l all-nodes.log
```

## Exit status

0 normally or on Ctrl+C; 1 for malformed payloads, missing/duplicate/out-of-order
nonzero fragments, a level change within a message, buffer limits or output
errors; 2 for usage/settings errors; 3 for connection failure/loss.
4 if `--count` is unmet or unfinished messages remain when
`--idle`/`--duration` expires. Stop timers should exceed `--ready-timeout`.
Unfinished messages are discarded with a stderr notification, not printed
as ready messages. Start reception before the first fragment.

---

[filename](./bottom_copyright.md ':include')
