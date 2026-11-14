# vscp-send

Send VSCP events.

```
vscp-send [OPTIONS] CLASS TYPE [DATA ...]
vscp-send [OPTIONS] -E EVENT [-E EVENT ...]
vscp-send [OPTIONS] --file FILE|-
```

## Options

| Option | Description |
| :----- | :---------- |
| `-g, --guid GUID` | GUID of the event. Default all zero, which tells the interface/server to use its own GUID |
| `-p, --priority 0-7` | Priority, 0 is the highest (default 3) |
| `-E, --event EVENT` | An event in the string, JSON or XML [format](formats.md). Repeatable |
| `-f, --file FILE` | Events from a file, one per line (`-` is stdin). Lines starting with `#` are skipped |
| `--no-check` | Allow more than 8 data bytes in a Level I class (below 512) |
| `-n, --count N` | Send everything N times. 0 sends until Ctrl+C |
| `-d, --delay MS` | Delay between events |
| `--linger MS` | Wait before disconnecting so queued events get out (default 500 ms for mqtt) |
| `--echo` | Print each event as it is sent |

Plus the [connection options](connecting.md).

Level I events (class below 512) have at most 8 data bytes; vscp-send refuses
more unless `--no-check` is given. Level II events can have up to 512 bytes.

## Examples

```bash
# CLASS1.INFORMATION ON, index 0, zone 1, subzone 2
vscp-send -i udp 20 3 0 1 2

# 23 °C from sensor 1 (CLASS1.MEASUREMENT, temperature), highest priority
vscp-send -i socketcan --device can0 -p 0 10 6 0x89 0x81 0x00 0xe6

# Data as hex pairs
vscp-send -i tcpip -H server 30 5 :000102

# A Level II event with a GUID: CLASS2.MEASUREMENT_STR temperature, sensor 0,
# zone 0, subzone 0, unit 1 (degrees Celsius), value "23.5"
vscp-send -i tcpip -H server -g FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:01 1040 6 0 0 0 1 :32332E35

# The same event ten times, 100 ms apart
vscp-send -i multicast -n 10 -d 100 20 3 0 1 2

# Record and play back
vscp-dump -i socketcan --device can0 -f string -n 100 > events.txt
vscp-send -i tcpip -H server --file events.txt -d 10

# Pipe events in
cat events.json | vscp-send -i mqtt -H broker --file -

# Encrypted
vscp-send -i udp -e aes128 -k $VSCP_KEY --hmac-key $VSCP_HMAC_KEY 20 3 0 1 2
vscp-send -i tcpip -H server --tls --ca-file ca.crt 20 3 0 1 2
```

## Exit status

0 when all events were sent, 1 when a send failed, 2 for command line
errors, 3 when the connection failed.
