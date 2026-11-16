# vscp-scan

Find the VSCP nodes on a segment.

```
vscp-scan [OPTIONS]
```

The default fast scan sends one *who is there* event (CLASS1.PROTOCOL,
type 31) to all nodes. Every node answers with its GUID and MDF URL, so one
request finds them all. Nodes that do not implement *who is there* are
found with `--slow`, which reads a register (the first GUID byte) of every
nickname in a range.

## Options

| Option | Description |
| :----- | :---------- |
| `-s, --slow` | Register read scan of the nicknames `--from` to `--to` |
| `--from N`, `--to N` | Nickname range for `--slow` (default 0-254) |
| `--timeout MS` | How long to wait for answers (default 2000, 3000 with `--slow`) |
| `--delay MS` | `--slow`: delay between the read requests (default 5) |
| `--via GUID` | GUID of the VSCP server interface to scan behind |
| `--info` | Also read the standard registers of each node found |
| `-j, --json` | JSON output |

Plus the [connection options](connecting.md).

## Example

```
$ vscp-scan -i socketcan --device can0 --info
Node  GUID                                             MDF URL
  7   FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:07  http://www.grodansparadis.com/paris/paris.xml
      firmware 1.1.2, manufacturer id 0x00000000/0x00000000, bootloader VSCP, 1 page(s)
 42   FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:2A  http://www.grodansparadis.com/kelvin/kelvin.xml
      firmware 1.0.4, manufacturer id 0x00000000/0x00000000, bootloader PIC1, 1 page(s)
2 node(s) found
```

```bash
# Over a VSCP daemon, nicknames 1-32 only
vscp-scan -i tcpip -H server --slow --from 1 --to 32

# For scripts
vscp-scan -i udp -j | jq -r '.[].nickname'
```

## GUID byte order in *who is there* answers

The specification lists the GUID bytes of the answer as GUID15 ... GUID0,
most significant byte first, while the VSCP firmware sends the least
significant byte first. vscp-scan decodes the answers as the firmware sends
them. With `--info` the GUID is read from the node's registers instead
(0xD0-0xDF, most significant byte first), which does not depend on this.

## Exit status

0 when at least one node was found, 4 when no node answered, 3 no
connection.

---

[filename](./bottom_copyright.md ':include')