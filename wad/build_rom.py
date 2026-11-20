#!/usr/bin/env python3

import argparse
import struct
from pathlib import Path

from prepare_iwad import prepare_iwad, read_iwad


ROM_BASE = 0x0E000000
ROM_CAPACITY = 4 * 1024 * 1024
ROM_ALIGNMENT = 4096
ROM_VECTORS = 0x80


def build_rom(engine: bytes, iwad: bytes) -> bytes:
    if len(engine) < ROM_ALIGNMENT or len(engine) % ROM_ALIGNMENT:
        raise ValueError("image du moteur Loopy mal alignée")

    first, last = struct.unpack_from(">II", engine)
    if first != ROM_BASE + ROM_VECTORS or not first <= last < ROM_BASE + len(engine):
        raise ValueError("en-tête du moteur Loopy invalide")
    if last & 1:
        raise ValueError("longueur du moteur Loopy invalide")

    engine_end = last - ROM_BASE + 2
    if engine_end + (-engine_end % ROM_ALIGNMENT) != len(engine):
        raise ValueError("le moteur doit être fourni sans IWAD ajouté")
    if any(byte != 0xFF for byte in engine[engine_end:]):
        raise ValueError("remplissage de l'image du moteur Loopy invalide")

    wad = prepare_iwad(iwad)
    used = len(engine) + len(wad)
    padding = -used % ROM_ALIGNMENT
    if used + padding > ROM_CAPACITY:
        raise ValueError(f"la ROM requiert {used:,} octets, limite {ROM_CAPACITY:,}".replace(",", " "))

    rom = bytearray(engine)
    rom.extend(wad)
    rom.extend(b"\xff" * padding)

    # The cartridge header stores the address of the last checked 16-bit word.
    struct.pack_into(">I", rom, 4, ROM_BASE + used - 2)
    checked = memoryview(rom)[ROM_VECTORS:used]
    checksum = sum(word for (word,) in struct.iter_unpack(">H", checked)) & 0xFFFFFFFF
    struct.pack_into(">I", rom, 8, checksum)
    return bytes(rom)


def main() -> None:
    parser = argparse.ArgumentParser(description="Assemble la ROM DOOM pour la Casio Loopy.")
    parser.add_argument("moteur", type=Path, help="image binaire du moteur fournie")
    parser.add_argument("iwad", type=Path, help="DOOM1.WAD ou DOOM19S.ZIP (shareware 1.9)")
    parser.add_argument("rom", type=Path, help="image cartouche à créer")
    args = parser.parse_args()
    if args.rom.resolve() in (args.moteur.resolve(), args.iwad.resolve()):
        parser.error("le fichier ROM doit être distinct des fichiers d'entrée")

    try:
        rom = build_rom(args.moteur.read_bytes(), read_iwad(args.iwad))
        args.rom.write_bytes(rom)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
