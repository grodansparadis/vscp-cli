# vscp-gen

Generate VSCP events for testing: random, fixed or counting class, type,
priority, length and data. Like `cangen` for CAN.

```
vscp-gen [OPTIONS]
```

## Options

| Option | Description |
| :----- | :---------- |
| `-C, --class N\|r` | Class, or `r` for random (default) |
| `-T, --type N\|r` | Type, or `r` (default) |
| `-p, --priority N\|r` | Priority 0-7, or `r` (default) |
| `-L, --length N\|r` | Number of data bytes, or `r` (default) |
| `-D, --data BYTES\|r\|i` | Fixed data, `r` random (default) or `i` an incrementing counter (little endian in the first 8 bytes) |
| `--guid GUID` | GUID of the events (default all zero) |
| `-2, --level2` | Random classes up to 1023 and up to 128 data bytes. Default: Level I classes (0-511) and 0-8 bytes |
| `--allow-protocol` | Allow the protocol classes |
| `-g, --gap MS` | Gap between events (default 200). 0 sends as fast as possible |
| `-b, --burst N` | Events sent back to back each gap (default 1) |
| `-n, --count N` | Stop after N events (default: until Ctrl+C) |
| `--seed N` | Random seed, gives the same sequence each time |
| `--echo` | Print each event as it is sent |

Plus the [connection options](connecting.md).

## Protocol classes are left out

Random events never use CLASS1.PROTOCOL (0), CLASS2.LEVEL1.PROTOCOL (512) or
CLASS2.PROTOCOL (1024) unless `--allow-protocol` is given. Events in those
classes can write registers, change nicknames or start the bootloader of real
nodes. A fixed `-C 0` also needs `--allow-protocol`.

## Examples

```bash
# Random Level I events, five per second
vscp-gen -i socketcan --device vcan0

# Load test: 10000 events as fast as possible
vscp-gen -i udp -g 0 -n 10000

# Temperature measurements with a counter as data, every 10 ms
vscp-gen -i tcpip -H server -C 10 -T 6 -L 4 -D i -g 10

# Level II events over multicast, shown as they are sent
vscp-gen -i multicast --level2 --echo

# The same "random" sequence every time
vscp-gen -i udp --seed 42 -n 100
```

---

[filename](./bottom_copyright.md ':include')