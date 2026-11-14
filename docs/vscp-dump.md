# vscp-dump

Show received VSCP events, one per line, until Ctrl+C.

```
vscp-dump [OPTIONS]
```

## Options

| Option | Description |
| :----- | :---------- |
| `-f, --format FMT` | `text` (default), `string`, `json`, `xml` or `csv`. See [formats](formats.md) |
| `-t, --time MODE` | Time stamp in text and csv: `a` absolute local time (default), `d` delta, `z` from the first event, `n` none |
| `-l, --log FILE` | Also append the output to FILE |
| `-q, --quiet` | Nothing on stdout (with `--log` or `--count`) |
| `-C, --class LIST` | Only these classes, e.g. `10,20`. Repeatable |
| `-T, --type LIST` | Only these types. Repeatable |
| `--from GUID` | Only events from this GUID |
| `--filter FILTER`, `--mask MASK` | VSCP filter and mask, `priority,class,type,GUID` |
| `-n, --count N` | Exit after N events |
| `--idle MS` | Exit when no event has arrived for MS milliseconds |
| `--duration MS` | Exit after MS milliseconds |

Plus the [connection options](connecting.md).

## Examples

```bash
# Everything on a CAN bus, time between events
vscp-dump -i socketcan --device can0 -t d

# Measurements from a VSCP daemon, as JSON lines
vscp-dump -i tcpip -H server -C 10,15,1040 -f json

# Encrypted UDP, plain datagrams are dropped
vscp-dump -i udp -e aes128 -k $VSCP_KEY --hmac-key $VSCP_HMAC_KEY --require-encryption

# MQTT topics of a site, log to a file for later replay with vscp-send --file
vscp-dump -i mqtt -H broker --subscribe 'site1/vscp/#' -f string -l site1.log

# In a script: wait at most 10 s for one heartbeat (CLASS1.INFORMATION, type 9)
if vscp-dump -i udp -C 20 -T 9 -n 1 --duration 10000 -q; then echo alive; fi

# CSV for a spreadsheet
vscp-dump -i multicast -f csv --duration 60000 > minute.csv
```

## Exit status

0 normally (Ctrl+C, `--count` reached, `--idle` or `--duration` passed).
4 when `--count` events did not arrive before `--idle`/`--duration`. 3 when the
connection failed or was lost.
