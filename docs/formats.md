# Event formats and filters

## Numbers

Classes, types, register numbers and data bytes are decimal (`20`) or hex
(`0x14`). Data can be separate arguments (`0 1 2`), one comma separated list
(`0,1,0x02`) or a run of hex pairs after a colon (`:000102`).

GUIDs are 16 hex bytes separated by colons:
`FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:01`.

## Output formats (vscp-dump -f)

### text (default)

For reading. With a time stamp (see `-t`), class, type, priority, GUID, the
number of data bytes and the data. Measurement events also show the value,
unit and sensor index.

```
2026-11-14T16:42:23.918  class=10   type=6   prio=3 FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07  [4] 89 81 00 E6  (measurement 23, unit 1, sensor 1)
```

### string

The VSCP string format, one event per line:

```
head,class,type,obid,datetime,timestamp,GUID,data1,data2,...
96,10,6,0,,0,FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07,0x89,0x81,0x00,0xE6
```

### json

One JSON object per line ("JSON lines"):

```json
{"class":10,"data":[137,129,0,230],"datetime":"","guid":"FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07","head":352,"note":"","obid":0,"timestamp_ns":"0x...","type":6}
```

### xml

One `<event .../>` element per line.

### csv

A header line, then one line per event. Easy to load in a spreadsheet.

```
time,head,class,type,obid,guid,size,data
2026-11-14T16:42:23.918,96,10,6,0,FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07,4,89 81 00 E6
```

## Time stamps (vscp-dump -t)

| Mode | Shows |
| :--- | :---- |
| `a` | Local time when the event was received (default) |
| `d` | Seconds since the previous event |
| `z` | Seconds since the first event |
| `n` | No time stamp |

## Input formats (vscp-send -E and --file)

`vscp-send` reads events in the string, JSON or XML format, so the output of
`vscp-dump -f string`, `-f json` or `-f xml` can be sent again. In the string
format a GUID of `-` means all zero (the interface fills in its GUID). Empty
date/time fields are fine.

```
# A comment
0,20,3,0,,0,-,0,1,2
{"class": 30, "type": 5, "guid": "-", "data": [7, 8]}
```

## Priority

The priority is bits 5-7 of the header (`head`), 0 is the highest. `vscp-send
-p` sets it; on CAN it is the top bits of the CAN id.

## Filters (vscp-dump)

The simple selections are usually enough:

```bash
vscp-dump -i udp -C 10,15        # only CLASS1.MEASUREMENT and CLASS1.DATA
vscp-dump -i udp -C 20 -T 3,4    # only CLASS1.INFORMATION ON/OFF
vscp-dump -i udp --from FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07
```

The VSCP filter/mask pair gives full control. Both have the form
`priority,class,type,GUID`. A mask bit set to 1 means the bit must equal the
filter bit:

```bash
# class 10, any type, any sender
vscp-dump -i udp --filter 0,10,0,00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00 \
                 --mask   0,0xffff,0,00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
```

The filtering is done by vscp-dump, so it works the same with all interfaces.

---

[filename](./bottom_copyright.md ':include')