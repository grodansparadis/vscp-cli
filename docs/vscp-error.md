# vscp-error

Show CLASS1.ERROR (508) events in readable text.

```
vscp-error [OPTIONS]
```

Each event prints local reception time, full source GUID, numeric type and
its description (for example **Channel error** or **Timeout error**),
index/channel, zone and subzone. Optional user-specific bytes are shown in
hexadecimal. Descriptions come from the pinned VSCP definitions.
Unknown types are explicitly marked `Unknown type`, retaining numeric type
and payload fields. Type 0 is **Success**, not an error condition.

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

Plus the [connection and security options](connecting.md). Without a selector
collect all nodes; both selectors together must match. Nicknames are not
globally unique, so prefer `--from` across multiple interfaces. A zero GUID
(`--from -`) is an exact filter. Only native Level I error events are decoded;
payloads must contain 3-8 bytes, including index, zone and subzone.

## Examples

```bash
vscp-error -i socketcan --device can0 -N 7
vscp-error -i tcpip -H server --from '{::01,07}' -l node7-errors.log
vscp-error -i udp -N 7 -n 1 --duration 10000
```

## Exit status

0 normally or on Ctrl+C; 1 for malformed payloads or output errors;
2 for usage/settings errors; 3 for connection failure/loss;
4 if `--count` is unmet when `--idle`/`--duration` expires.
Receiving an error event does not itself make the tool fail.

---

[filename](./bottom_copyright.md ':include')
