# Connecting and security

All tools share these options. Run any tool with `--help` to see them.

## Choosing the interface

```
-i, --interface TYPE   tcpip, udp, multicast, ws1, ws2, mqtt, socketcan, canal or level2
-H, --host HOST        Server, broker, broadcast address or multicast group
-P, --port PORT        Port
-u, --user USER        User name
    --password PW      Password (or set VSCP_PASSWORD)
```

Defaults when nothing is given:

| Interface | Default |
| :-------- | :------ |
| `tcpip` | `tcp://localhost:9598`, user `admin`, password `secret` |
| `udp` | The client default (broadcast on the VSCP UDP port 33333) |
| `multicast` | Group `224.0.23.158`, port 9598 |
| `ws1`, `ws2` | `ws://localhost:8884/ws1` or `/ws2`, user `admin`, password `secret` |
| `mqtt` | `localhost:1883` (8883 with `--tls`), subscribe `vscp/#`, publish `vscp/{{guid}}/{{class}}/{{type}}` |
| `socketcan` | Device `vcan0` |
| `canal`, `level2` | No default, `--driver` is needed |

The password `secret` is only used with the default user. Give a password
with `--password`, or better through the `VSCP_PASSWORD` environment variable
so it does not show up in the process list or the shell history.

### Interface specific options

| Option | Interfaces | Description |
| :----- | :--------- | :---------- |
| `--net-interface NAME` | udp, multicast | Network interface to send and join the group on, e.g. `eth0` |
| `--device DEV` | socketcan | SocketCAN device, e.g. `can0` |
| `--if-guid GUID` | socketcan, level2 | GUID of the interface (received SocketCAN events get it with the nickname as the last byte) |
| `--driver PATH` | canal, level2 | Driver library (`.so`, `.dylib` or `.dll`) |
| `--driver-config CFG` | canal, level2 | CANAL: the driver's configuration string. Level II: the driver's configuration file |
| `--flags N` | canal, socketcan | Driver flags |
| `--subscribe TOPIC` | mqtt | Topic to receive events on, repeatable |
| `--publish TOPIC` | mqtt | Topic to send events to, repeatable. `{{guid}}`, `{{class}}`, `{{type}}`, `{{nickname}}` and more are replaced |
| `--mqtt-format FMT` | mqtt | Payload of sent events: `json` (default), `xml`, `string` or `binary` |
| `--qos N` | mqtt | QoS 0-2 |
| `--connect-timeout MS` | all | How long to wait for the connection (default 5000) |
| `--response-timeout MS` | tcpip, ws1, ws2, socketcan | How long to wait for a server answer (default 3000) |

## Security

### TLS (tcpip, ws1, ws2, mqtt)

```
--tls              Use TLS (stcp:// for tcpip, wss:// for websockets)
--ca-file FILE     CA certificate(s) to verify the server with
--ca-path DIR      Directory with CA certificates (e.g. /etc/ssl/certs)
--cert FILE        Client certificate (PEM) for servers that ask for one
--cert-key FILE    Private key of the client certificate (PEM)
--insecure         Do not verify the server certificate - for testing only
```

The server certificate is always verified unless `--insecure` is given. A
server with a self-signed certificate needs that certificate as `--ca-file`.

```bash
# VSCP daemon with TLS
vscp-dump -i tcpip -H vscp.example.com --tls --ca-file /etc/vscp/certs/ca.crt

# MQTT broker with a certificate from a public CA
vscp-dump -i mqtt -H broker.example.com --tls --ca-path /etc/ssl/certs -u vscp

# Websocket server, client certificate
vscp-send -i ws2 -H vscp.example.com --tls --ca-file ca.crt --cert me.crt --cert-key me.key 20 3 0 1 2
```

MQTT needs `--ca-file` or `--ca-path` for TLS. On Linux `--ca-path /etc/ssl/certs`
uses the system certificates.

### Encryption and authentication (tcpip, udp, multicast)

```
-e, --encryption ALG    none, aes128, aes192 or aes256
-k, --key HEX           AES key: 32, 48 or 64 hex digits (or set VSCP_KEY)
    --hmac-key HEX      HMAC-SHA256 key: 64 hex digits (or set VSCP_HMAC_KEY)
    --require-encryption  udp/multicast: drop events that are not encrypted
```

* **udp and multicast** encrypt every datagram with AES in CBC mode. With
  `--hmac-key` each datagram also carries an HMAC-SHA256 so changed or forged
  datagrams are dropped. `--require-encryption` drops plain datagrams.
* **tcpip** uses the binary protocol for encryption. Both keys are needed; each
  record is AES encrypted and authenticated with HMAC-SHA256. `--binary`
  alone selects the binary protocol without encryption.
* **ws1/ws2** use `--key` (32 hex digits) as the key for the AES
  authentication of the login. It must match the server's key.

Keep keys out of the command line where you can:

```bash
export VSCP_KEY=000102030405060708090A0B0C0D0E0F
export VSCP_HMAC_KEY=1F1E1D1C1B1A191817161514131211100F0E0D0C0B0A09080706050403020100
vscp-dump -i udp -e aes128 --require-encryption
vscp-send -i udp -e aes128 20 3 0 1 2
```

TLS and encryption can be combined for tcpip:

```bash
vscp-send -i tcpip -H vscp.example.com --tls --ca-file ca.crt -e aes256 20 3 0 1 2
```

## Configuration files

All client settings can be kept in a JSON file and given with `-c`. The file
holds the settings of the VSCP client of the interface, plus `type` for the
interface. Command line options override what is in the file.

```json
{
  "type": "tcpip",
  "host": "stcp://vscp.example.com:9598",
  "user": "admin",
  "btls": true,
  "cafile": "/etc/vscp/certs/ca.crt",
  "binary-mode": true,
  "binary-encryption": 1,
  "binary-key": "000102030405060708090A0B0C0D0E0F",
  "binary-hmac-key": "1F1E1D1C1B1A191817161514131211100F0E0D0C0B0A09080706050403020100"
}
```

```bash
VSCP_PASSWORD=secret vscp-dump -c server.json
```

A VSCP simulator configuration file (with an `interface` object) can also be
used directly. The [examples](https://github.com/grodansparadis/vscp-cli/tree/main/examples)
folder has a file for each interface. They are installed in
`share/vscp-cli/examples`.

`--set KEY=VALUE` sets any single client setting. The value is JSON if it
parses as JSON, otherwise text:

```bash
vscp-dump -i udp --set require-encryption=true --set interface=eth1
```

## Checking the settings

`--print-config` prints the settings the client gets, with the password and
keys masked, and exits without connecting:

```bash
$ vscp-send -i tcpip -H server --tls --ca-file ca.crt -e aes128 --print-config
{
  "type": "tcpip",
  "binary-encryption": 1,
  "binary-hmac-key": "********",
  "binary-key": "********",
  "binary-mode": true,
  "btls": true,
  "cafile": "ca.crt",
  ...
}
```

`-v` shows the library log on stderr (`-vv` debug, `-vvv` trace).

## Notes for each interface

### udp

UDP events are sent to `--host` (a host or a broadcast address) and received
on the same port. Several programs on one computer can listen on the port at
the same time. A unicast datagram reaches only one of them, so use a broadcast
address to reach all; on Linux `127.255.255.255` reaches the programs on the
same computer only.

### multicast

The client joins the group and receives from it. `--net-interface` selects
the network interface when the computer has more than one.

### socketcan

Linux only. Set the bit rate with `ip link`, e.g.

```bash
sudo ip link set can0 type can bitrate 125000
sudo ip link set can0 up
```

A virtual bus for testing:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
```

### canal and level2

The driver is loaded at run time. The driver's documentation describes its
configuration.

```bash
vscp-scan -i canal --driver /usr/lib/vscp/drivers/level1/vscpl1drv-can4vscp.so \
          --driver-config "/dev/ttyUSB0;125"
vscp-dump -i level2 --driver /usr/lib/vscp/drivers/level2/libvscpl2drv-socketcan.so \
          --driver-config /etc/vscp/socketcan.json
```

### Nodes behind a VSCP server interface

`vscp-reg`, `vscp-info` and `vscp-scan` take `--via GUID`, the GUID of the
interface on a VSCP server that a node is connected to. The register events
are then sent as CLASS2.LEVEL1.PROTOCOL to that interface.
