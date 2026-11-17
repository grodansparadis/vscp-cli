# vscp-diagnostic

Show CLASS1.DIAGNOSTIC (506) events in readable text.

```
vscp-diagnostic [OPTIONS]
```

Each event prints local reception time, full source GUID, numeric type and
its description (for example **Overvoltage** or **Overheat**), index/channel,
zone and subzone. Optional user-specific bytes are shown in hexadecimal.
Descriptions come from the pinned VSCP definitions; unknown types are explicitly
marked `Unknown type` and retain their numeric type and payload fields.

## Options

| Option | Description |
| :----- | :---------- |
| `--from GUID` | Only this exact full GUID, in any supported VSCP GUID form |
| `-N, --node NICKNAME` | Only this last GUID byte, 0-255, decimal or `0x` hexadecimal |
| `-l, --log FILE` | Also append displayed records to FILE |
| `-q, --quiet` | No stdout output |
| `-n, --count N` | Exit after N displayed events; 0 (default) means unlimited |
| `--idle MS` | Exit after MS without selected events; 0 (default) means unlimited |
| `--duration MS` | Exit after MS; 0 (default) means unlimited |

Plus the [connection and security options](connecting.md). Without a node
selector all nodes are collected. Both selectors together must match.
Nicknames can occur on multiple interfaces; use `--from` to disambiguate.
An explicitly supplied zero GUID (`--from -`) is an exact filter, not a wildcard.
Only native Level I diagnostic events are decoded; payloads must contain
3-8 bytes (index, zone, subzone and optional user information).

## Examples

```bash
vscp-diagnostic -i socketcan --device can0 -N 7
vscp-diagnostic -i tcpip -H server --from '{::01,07}' -l node7-diagnostics.log
vscp-diagnostic -i udp -N 7 -n 1 --duration 10000
```

## Exit status

0 normally or on Ctrl+C; 1 for malformed payloads or output errors;
2 for usage/settings errors; 3 for connection failure/loss;
4 if `--count` is unmet when `--idle`/`--duration` expires.
Receiving a diagnostic event does not itself make the tool fail.

---

[filename](./bottom_copyright.md ':include')
