#!/usr/bin/env python3

import argparse
import io
import struct
import zipfile
from pathlib import Path


HEADER = struct.Struct("<4sII")
ENTRY = struct.Struct("<II8s")


def read_iwad(path: Path) -> bytes:
    data = path.read_bytes()
    if data[:4] != b"PK\x03\x04":
        return data

    # The original DOS 1.9 shareware installer distributes a split ZIP SFX.
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as installer:
            parts = [installer.read(f"DOOMS_19.{index}") for index in (1, 2)]
        with zipfile.ZipFile(io.BytesIO(b"".join(parts))) as payload:
            return payload.read("DOOM1.WAD")
    except (KeyError, zipfile.BadZipFile, RuntimeError) as error:
        raise ValueError("archive d'installation DOOM shareware 1.9 invalide") from error


def prepare_iwad(source: bytes) -> bytes:
    if len(source) < HEADER.size:
        raise ValueError("en-tête WAD incomplet")

    kind, count, directory = HEADER.unpack_from(source)
    if kind != b"IWAD":
        raise ValueError("le fichier doit être un IWAD")
    if count == 0 or directory < HEADER.size or directory > len(source):
        raise ValueError("répertoire IWAD invalide")
    if count > (len(source) - directory) // ENTRY.size:
        raise ValueError("répertoire IWAD tronqué")

    result = bytearray(HEADER.size)
    entries = bytearray()
    relocated = {}
    for index in range(count):
        position, size, name = ENTRY.unpack_from(source, directory + index * ENTRY.size)
        if size and (position < HEADER.size or position > len(source) or size > len(source) - position):
            raise ValueError(f"entrée IWAD {index} hors du fichier")
        # The current console port has no music or sound effect playback.
        if size == 0 or name.startswith((b"D_", b"DS", b"DP")):
            entries.extend(ENTRY.pack(0, 0, name))
            continue

        alignment = 256 if name == b"COLORMAP" else 4
        key = (position, size, alignment)
        aligned = relocated.get(key)
        if aligned is None:
            result.extend(b"\0" * (-len(result) % alignment))
            aligned = len(result)
            result.extend(source[position:position + size])
            relocated[key] = aligned
        entries.extend(ENTRY.pack(aligned, size, name))

    result.extend(b"\0" * (-len(result) % 4))
    HEADER.pack_into(result, 0, kind, count, len(result))
    result.extend(entries)
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser(description="Prépare un IWAD pour la Casio Loopy.")
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.destination.resolve():
        parser.error("les chemins d'entrée et de sortie doivent être différents")

    try:
        aligned = prepare_iwad(read_iwad(args.source))
        args.destination.write_bytes(aligned)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
