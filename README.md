# pingle 🌱

> *Ping as much, pollute less*

Minimal ICMP ping written in C — 8-byte packets, ~5 KB in memory.

## Why pingle?

Standard `ping` (iputils) carries decades of features that typical usage never touches.
pingle strips it down to the essentials:

| Metric | pingle | iputils | Ratio |
|---|---|---|---|
| Binary size | 17 KB | 153 KB | 9× smaller |
| ICMP packet | 8 bytes | 64 bytes | 8× less traffic |
| Heap peak | ~6 MB | ~12 MB | 2× less memory |
| RSS peak | ~2 MB | ~3 MB | 1.5× less RAM |

Multiplied across millions of daily executions on millions of servers,
every saved byte has a real energy impact.

## Requirements

- Linux kernel ≥ 3.11 (ICMP `SOCK_DGRAM` support)
- `net.ipv4.ping_group_range` covering your GID (default on most distributions)
- GCC, GNU make

## Build

```bash
make              # release binary
make debug        # debug + AddressSanitizer + UBSan
make strip        # strip symbols, print section sizes
```

## Install

```bash
sudo make install              # installs to /usr/local/bin
sudo make install PREFIX=/usr  # installs to /usr/bin
sudo make uninstall            # removes the binary
```

## Usage

```bash
pingle <destination>
```

```
PING google.com (142.250.75.238) 🐧
8 bytes from xxx.xxx.xxx.xxx: icmp_seq=1 time=12.34 ms
8 bytes from xxx.xxx.xxx.xxx: icmp_seq=2 time=11.87 ms
^C
--- google.com ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 2001ms
rtt min/avg/max/mdev = 11.870/12.105/12.340/0.235 ms
```

Press **Ctrl-C** to stop and display statistics.

## License

GNU General Public License v3 — see [LICENSE](LICENSE) or <https://www.gnu.org/licenses/>.

Derived from work by Mike Muuss (original `ping`, 1983).
