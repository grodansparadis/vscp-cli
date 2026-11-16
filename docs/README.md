# VSCP command line tools

Small programs for working with a [VSCP](https://www.vscp.org) system from a
terminal or a script. They work the same way with every VSCP transport and on
Linux (also Raspberry Pi), macOS and Windows.

| Tool | What it does |
| :--- | :----------- |
| [vscp-send](vscp-send.md) | Send events, from the command line or replayed from a file |
| [vscp-dump](vscp-dump.md) | Show received events (text, VSCP string, JSON, XML or CSV) |
| [vscp-gen](vscp-gen.md) | Generate random, fixed or counting events for testing |
| [vscp-reg](vscp-reg.md) | Read and write the registers of a node |
| [vscp-info](vscp-info.md) | Show the GUID, MDF URL, firmware version etc. of a node, download its MDF |
| [vscp-scan](vscp-scan.md) | Find the nodes on a segment |

The ideas come from the `can-utils` tools for SocketCAN (`cansend`, `candump`,
`cangen`): one small program per job, plain text output, useful exit codes.

## Interfaces

All tools take the same [connection options](connecting.md). `-i` selects the
interface:

| Interface | Transport | Security |
| :-------- | :-------- | :------- |
| `tcpip` | VSCP tcp/ip link protocol to a VSCP daemon or node, text or binary mode | TLS, AES-128/192/256 + HMAC-SHA256 (binary mode), user/password |
| `udp` | VSCP over UDP (unicast or broadcast) | AES-128/192/256 + HMAC-SHA256 |
| `multicast` | VSCP multicast | AES-128/192/256 + HMAC-SHA256 |
| `ws1`, `ws2` | VSCP websocket protocols | TLS (wss), user/password with AES authentication |
| `mqtt` | VSCP events over an MQTT broker | TLS, client certificates, user/password |
| `socketcan` | VSCP Level I over SocketCAN (Linux only) | |
| `canal` | A CANAL (Level I) driver, e.g. for a CAN4VSCP adapter | |
| `level2` | A VSCP Level II driver | |

## A first try

```bash
# Show everything on a CAN bus
vscp-dump -i socketcan --device can0

# Turn something on: CLASS1.CONTROL, TurnOn, zone 1, subzone 2
vscp-send -i socketcan --device can0 30 5 0 1 2

# Which nodes are there?
vscp-scan -i socketcan --device can0

# All about node 7, including what its MDF says
vscp-info -i socketcan --device can0 -N 7 --mdf

# Set register 0x10 of node 7 to 0x55
vscp-reg -i socketcan --device can0 -N 7 write 0x10 0x55

# 1000 random events for testing, over multicast
vscp-gen -i multicast -g 10 -n 1000
```

The same commands work over the network, for example through a VSCP daemon
with TLS and encryption:

```bash
vscp-dump -i tcpip -H vscp.example.com --tls --ca-file ca.crt \
          -e aes128 -k $AESKEY --hmac-key $HMACKEY

# Encrypted UDP, plain datagrams are dropped
vscp-dump -i udp -e aes128 --require-encryption
```

The [connection options](connecting.md) page covers all interfaces, TLS and
encryption, and JSON configuration files that store the settings (and even
the command) of each tool:

```bash
vscp-dump -c site.json        # interface, TLS and dump options all from site.json
vscp-reg  -c site.json -N 8   # stored settings, node from the command line
```

## Exit status

All tools use the same exit status, so scripts can tell what went wrong:

| Status | Meaning |
| :----: | :------ |
| 0 | OK |
| 1 | The operation failed (send failed, register is read only, MDF could not be fetched) |
| 2 | Command line error (unknown option, invalid value, invalid interface settings) |
| 3 | Could not connect (or the connection was lost) |
| 4 | No answer from the node / no node found / fewer events than `--count` arrived |

## License

MIT. See [License](https://github.com/grodansparadis/vscp-cli/blob/main/LICENSE).
