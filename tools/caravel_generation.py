#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Read-only discovery shared by the 1.x command-line update tools."""
import time

CLASSIC_HEADER = bytes([0xF0, 0x7D, 0x46, 0x4C])
V1_HEADER = bytes([0xF0, 0x7D, 0x43, 0x41])
PROTOCOL = 18


def info(data):
    at = 0
    def byte():
        nonlocal at
        if at >= len(data):
            raise ValueError("Short generation INFO")
        v = data[at]
        at += 1
        return v
    def string():
        out = bytearray()
        while (v := byte()):
            out.append(v)
        return out.decode("ascii")
    version = string()
    count = byte()
    counts = [byte() for _ in range(4)]
    if not 1 <= count <= 32 or not counts[0] or not counts[1]:
        raise ValueError("Invalid generation counts")
    engines = [string() for _ in range(count)]
    if byte() != 4:
        raise ValueError("Unsupported track protocol")
    rows, tags = byte(), {}
    while at < len(data):
        tag, rev = byte(), byte()
        if tag in tags:
            raise ValueError("Duplicate generation capability")
        tags[tag] = [rev] + [byte() for _ in range(2 if tag in (0x4d, 0x46, 0x41) else 1)]
    if 0x60 in tags:
        if rows or tags[0x60] != [2, PROTOCOL] or engines != ["ANALOG", "PHYS", "SEM", "HADES", "DRONE", "ENSEMBLE"]:
            raise ValueError("Unsupported 1.x protocol revision")
        return 2, version
    if rows != 16 or tags.get(0x55, [0])[0] != 1 or tags.get(0x42, [0])[0] != 1:
        raise ValueError("Unsupported Classic protocol")
    return 1, version


def request(link, header, cmd, args=b"", timeout=0.7):
    link.drain()
    if not link.send(header + bytes([cmd]) + args + b"\xF7"):
        raise OSError("MIDI send failed")
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        reply = link.read(end - time.monotonic())
        if reply and reply.startswith(header + bytes([cmd])) and reply[-1] == 247:
            return reply[5:-1]
    raise TimeoutError("No matching generation protocol; nothing written")


def identify(link):
    try:
        gen, version = info(request(link, V1_HEADER, 1))
        if gen != 2:
            raise ValueError("Wrong generation on 1.x transport")
        header = V1_HEADER
    except (ValueError, TimeoutError):
        gen, version = info(request(link, CLASSIC_HEADER, 1))
        if gen != 1:
            raise ValueError("Wrong generation on Classic transport")
        header = CLASSIC_HEADER
    catalog = request(link, header, 65)
    ids = tuple(range(8)) if gen == 2 else (*range(10), 32, 33, 34)
    if (len(catalog) != 3 + 11 * len(ids) or catalog[:3] != bytes([gen, 0, len(ids)])
            or tuple(catalog[3::11]) != ids):
        raise ValueError("INFO/LIST generation mismatch; nothing written")
    return gen, version, header


def update_entry(generation):
    if generation == 1:
        return bytes([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7])
    if generation == 2:
        return V1_HEADER + bytes([0x7F, PROTOCOL, 2, 0xF7])
    raise ValueError("Unknown generation; update refused")
