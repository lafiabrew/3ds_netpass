# NetPass (`3ds_netpass`)

> **Play Coins & StreetPass** — Nintendo 3DS Homebrew (`CTR-P-SPAS`, UniqueId `0x0D3E6`) by [**lafiabrew**](https://github.com/orgs/lafiabrew/repositories)

Nintendo 3DS Play Coin maxer (300 coins with .bak backup), pedometer step reader, and StreetPass CECD box archiver.

## Build & Verify

```bash
# Run host C11 ASan/UBSan unit & UI simulation tests
make test

# Build ARM11 3DS .elf, .smdh, .3dsx, and .cia (via devkitARM or rootless Podman container)
make 3ds && make verify-3ds

# Serve FBI QR code or push .cia over LAN
make qr
```
