# vscp-reg

Read and write the registers of a VSCP node with the VSCP register protocol.

```
vscp-reg [OPTIONS] -N NODE read REG [COUNT]
vscp-reg [OPTIONS] -N NODE write REG VALUE [VALUE ...]
vscp-reg [OPTIONS] -N NODE dump [user|standard|all]
```

Registers `0x00`-`0x7F` are the node's own (user) registers on the selected
page. `0x80`-`0xFF` are the standard registers, the same on every page
(GUID, MDF URL, firmware version, ...). See the
[register model](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_register_abstraction_model)
in the specification.

## Options

| Option | Description |
| :----- | :---------- |
| `-N, --node NICKNAME` | The node: its nickname (0-255) or its GUID |
| `--page N` | Register page for registers 0x00-0x7F (default 0) |
| `--via GUID` | GUID of the VSCP server interface the node is on |
| `--timeout MS` | Timeout for each register operation (default 2000) |
| `-j, --json` | JSON output |
| `-q, --quiet` | No output from write |

Plus the [connection options](connecting.md).

## Commands

**read REG [COUNT]** reads COUNT registers (default 1) from REG.

```
$ vscp-reg -i socketcan --device can0 -N 7 read 0 4
0000:00  0x11   17
0000:01  0x22   34
0000:02  0x00    0
0000:03  0x00    0
```

**write REG VALUE ...** writes the values to REG, REG+1, ... A write is
confirmed by the node answering with the new value. When it answers with
another value the register is read only (or the value was limited) and
vscp-reg says so:

```
$ vscp-reg -i socketcan --device can0 -N 7 write 0x10 0x55
10 <- 0x55 ok
$ vscp-reg -i socketcan --device can0 -N 7 write 0x81 5
vscp-reg: register 0x81 is 0x02 after writing 0x05 (read only or limited)
```

**dump** shows all registers as a hex dump: `user` (0x00-0x7F of the page),
`standard` (0x80-0xFF) or `all` (default).

```
$ vscp-reg -i udp -N 42 dump standard
           0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
     80   00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
     90   00 2A 00 00 00 01 02 00 08 01 00 00 00 00 00 00  .*..............
     ...
```

## Examples

```bash
# The zone and subzone of node 7 (registers 0 and 1 in many nodes)
vscp-reg -i socketcan --device can0 -N 7 read 0 2

# All user registers on page 1 as JSON
vscp-reg -i tcpip -H server -N 0x12 --page 1 read 0 128 -j

# Node behind interface 3 of a VSCP daemon
vscp-reg -i tcpip -H server --via FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:03:00:00 -N 7 dump user

# Over encrypted UDP
vscp-reg -i udp -e aes128 -k $VSCP_KEY --hmac-key $VSCP_HMAC_KEY -N 1 write 0 1
```

## Exit status

0 OK, 1 a register could not be written (read only), 2 command line error,
3 no connection, 4 the node did not answer.

---

[filename](./bottom_copyright.md ':include')