#!/usr/bin/env python3
"""Talk to the Fellow kettle over BLE through the Flair ESP (ESPHome active Bluetooth proxy).

Runs on any machine on the LAN; the ESP next to the kettle does the radio work, so protocol
experiments need no firmware change. Uses aioesphomeapi's GATT calls directly (no CoreBluetooth).

  python kettle_proxy.py scan               # show kettle adverts seen by the ESP
  python kettle_proxy.py dump               # connect, list GATT, read safe characteristics
  python kettle_proxy.py listen 30          # dump + print notifications for 30 s
  python kettle_proxy.py write <uuid> TEXT  # write TEXT + "\\n" (needs --yes; commands change state)

Env: ESP_HOST (default flair58.local), ESP_SECRETS = path to the ESPHome secrets.yaml with api_key
(the key is read from the file and never printed).

The ESP needs an active Bluetooth proxy, which esphome/flair58.yaml does not enable by default. Add for the test:
  esp32_ble_tracker:
  bluetooth_proxy:
    active: true
Requires: pip install aioesphomeapi
"""
from __future__ import annotations

import asyncio
import os
import sys
import time

from aioesphomeapi import APIClient, BluetoothScannerMode

NAMES = ("EKG", "Stagg", "Fellow")
SERVICE_UUIDS = ("7aebf330-6cb1-46e4-b23b-7cc2262c605e", "021a9004-0382-4aea-bff4-6b3f1c5adfb4")
TEXT_CHARS = {"2291c4b7", "2291c4b8", "2291c4b9"}  # firmware, device name, Wi-Fi MAC (ASCII)
CCCD = "00002902-0000-1000-8000-00805f9b34fb"
PROPS = {0x02: "read", 0x04: "write-nr", 0x08: "write", 0x10: "notify", 0x20: "indicate"}


def log(*parts):
    print(time.strftime("%H:%M:%S"), *parts, flush=True)


def api_key() -> str:
    path = os.environ["ESP_SECRETS"]
    for line in open(path, encoding="utf-8"):
        if line.startswith("api_key:"):
            return line.split(":", 1)[1].strip().strip('"').strip("'")
    raise SystemExit("api_key not found in ESP_SECRETS")


# 128-bit service UUIDs appear little-endian in the raw advert.
UUID_BYTES = tuple(bytes.fromhex(u.replace("-", ""))[::-1] for u in SERVICE_UUIDS)


def ad_name(data: bytes) -> str:
    i = 0
    while i + 1 < len(data):
        length = data[i]
        if length == 0 or i + 1 + length > len(data):
            break
        kind, value = data[i + 1], data[i + 2 : i + 1 + length]
        if kind in (0x08, 0x09):
            return value.decode("utf-8", "replace")
        i += 1 + length
    return ""


def mac(address: int) -> str:
    return ":".join(f"{b:02X}" for b in address.to_bytes(6, "big"))


async def find_kettle(cli: APIClient, seconds: float = 15):
    seen: dict[int, tuple[str, int, int]] = {}

    def on_ads(resp):
        for ad in resp.advertisements:
            data = bytes(ad.data)
            name = ad_name(data)
            hit = name.startswith(NAMES) or any(u in data for u in UUID_BYTES)
            if hit and ad.address not in seen:
                seen[ad.address] = (name, ad.rssi, ad.address_type)
                log("ADVERT", mac(ad.address), repr(name), "rssi", ad.rssi, "type", ad.address_type)

    # Names come in the scan response, which only an active scan requests.
    cli.bluetooth_scanner_set_mode(BluetoothScannerMode.ACTIVE)
    unsub = cli.subscribe_bluetooth_le_raw_advertisements(on_ads)
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline and not seen:
        await asyncio.sleep(0.5)
    # Keep the subscription: the proxy tears down every GATT connection as soon as
    # its (single) advertisement subscriber is gone.
    return next(iter(seen.items()), None)


async def main(argv: list[str]) -> None:
    cmd = argv[0] if argv else "dump"
    cli = APIClient(os.environ.get("ESP_HOST", "flair58.local"), 6053, None, noise_psk=api_key())
    await cli.connect(login=True)
    info = await cli.device_info()
    log("ESP", info.name, info.esphome_version, "proxy flags", info.bluetooth_proxy_feature_flags_compat(cli.api_version))
    try:
        kettle = await find_kettle(cli)
        if not kettle:
            log("no kettle advert within 15 s")
            return
        if cmd == "scan":
            return
        address, (_, _, address_type) = kettle
        flags = info.bluetooth_proxy_feature_flags_compat(cli.api_version)
        connected = asyncio.Event()

        def on_state(is_connected: bool, mtu: int, error: int) -> None:
            log("CONNECTION", "up" if is_connected else "down", "mtu", mtu, "error", error)
            if is_connected:
                connected.set()

        cancel = await cli.bluetooth_device_connect(address, on_state, timeout=30, feature_flags=flags,
                                                    has_cache=False, address_type=address_type)
        try:
            await asyncio.wait_for(connected.wait(), 30)
            services = await cli.bluetooth_gatt_get_services(address)
            chars = {}
            for s in services.services:
                log("SERVICE", s.uuid)
                for c in s.characteristics:
                    props = ",".join(n for bit, n in PROPS.items() if c.properties & bit)
                    log("  CHAR", c.uuid, "handle", c.handle, props)
                    chars[c.uuid] = c
            for uuid, c in chars.items():
                if c.properties & 0x02 and uuid[:8] in TEXT_CHARS | {"2291c4b1", "2291c4b5"}:
                    raw = bytes(await cli.bluetooth_gatt_read(address, c.handle))
                    shown = raw.split(b"\0")[0].decode("ascii", "replace") if uuid[:8] in TEXT_CHARS else raw.hex(" ")
                    log("READ", uuid[:8], shown)
            if cmd == "write" and "--yes" not in argv:
                raise SystemExit("write changes kettle state; add --yes")
            if cmd in ("listen", "write"):
                for uuid, c in chars.items():
                    # Kettle characteristics only; the generic Service Changed CCCD is rejected.
                    if c.properties & 0x30 and uuid.startswith("2291c4b"):
                        await cli.bluetooth_gatt_start_notify(address, c.handle, lambda h, d, u=uuid: log(
                            "NOTIFY", u[:8], bytes(d).decode("ascii", "replace") if u.startswith("2291c4b6") else bytes(d).hex(" ")))
                        cccd = next((d for d in c.descriptors if d.uuid == CCCD), None)
                        if cccd:
                            try:
                                await cli.bluetooth_gatt_write_descriptor(address, cccd.handle, b"\x01\x00")
                            except Exception as err:  # some firmware rejects the CCCD write
                                log("CCCD", uuid[:8], type(err).__name__)
                if cmd == "write":
                    target = next(u for u in chars if u.startswith(argv[1].lower()))
                    payload = (argv[2] + "\n").encode()
                    log("WRITE", target[:8], payload)
                    await cli.bluetooth_gatt_write(address, chars[target].handle, payload, True)
                seconds = float(argv[1]) if cmd == "listen" and len(argv) > 1 else 15
                log(f"listening {seconds:.0f} s")
                await asyncio.sleep(seconds)
        finally:
            await cli.bluetooth_device_disconnect(address)
            cancel()
    finally:
        await cli.disconnect()


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1:]))
