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
| `-2, --level2` | Include defined Level II classes up to 1060. Default: defined Level I classes (0-511) |
| `-m, --measurements` | Only encoded random measurements; use `-C` and `-T` to select class and type |
| `--allow-protocol` | Allow the protocol classes |
| `-g, --gap MS` | Gap between events (default 200). 0 sends as fast as possible |
| `-b, --burst N` | Events sent back to back each gap (default 1) |
| `-n, --count N` | Stop after N events (default: until Ctrl+C) |
| `--seed N` | Random seed, gives the same sequence each time |
| `--echo` | Print each event as it is sent |

Plus the [connection options](connecting.md).

## Valid class/type pairs and sizes

Only class/type pairs defined in the bundled VSCP catalog are generated;
reserved types are excluded. Fixing either class or type restricts the random
selection to matching pairs. An undefined pair or incompatible payload size is
a command line error, detected before connecting.

Level I classes (below 512) have 0-8 data bytes. Encapsulated Level I classes
(512-1023) have 16-24 bytes, including the 16-byte GUID prefix. Native Level II
classes have 0-512 bytes. These limits also apply to fixed data and lengths,
regardless of `--level2`. A fixed class can select Level II without `--level2`.
When both fixed data and length are given, their sizes must agree.

Outside measurement mode, payload bytes remain arbitrary test data: class/type
and size validation does not validate the meaning of the payload.

## Random measurements

`--measurements` supports classes 10, 60, 65 and 70, their encapsulated
equivalents 522, 572, 577 and 582, and native Level II classes 1040 and 1060.
By default it chooses among the four Level I classes; `--level2` includes the
Level II variants. `-C` fixes the class and `-T` fixes the measurement type.
Other classes are rejected in this mode.

Values range from 0 to 100 in steps of 0.01, using the default unit (for
temperature, Kelvin), sensor/index 0, zone 0 and subzone 0. Payloads use
normalized integers, IEEE-754 values or strings as required by the class.
Encapsulated events prefix the measurement with the event GUID.
Fixed `--length` and explicit `--data` options cannot be used in this mode.

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

# Random temperature measurements, every 10 ms (default unit: Kelvin)
vscp-gen -i tcpip -H server --measurements -C 10 -T 6 -g 10

# Native Level II floating point pressure measurements
vscp-gen -i udp --measurements -C 1060 -T 12

# Level II events over multicast, shown as they are sent
vscp-gen -i multicast --level2 --echo

# The same "random" sequence every time
vscp-gen -i udp --seed 42 -n 100
```

---

[filename](./bottom_copyright.md ':include')