#!/usr/bin/env python3
"""Read-only installation/PE identity audit; never imports or executes game code."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def audit(root):
    executable = root / 'gta_sa.exe'
    data = executable.read_bytes()
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Not a DOS/PE executable')
    offset = struct.unpack_from('<I', data, 60)[0]
    if offset + 120 > len(data) or data[offset:offset + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    optional = offset + 24
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('Expected PE32 optional header')
    machine = struct.unpack_from('<H', data, offset + 4)[0]
    assets = ['models/gta3.img', 'models/gta_int.img', 'models/player.img',
              'data/gta.dat', 'data/default.dat', 'data/script/main.scm',
              'anim/ped.ifp', 'text/american.gxt']
    return {
        'executable': {
            'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'machine': hex(machine),
            'entry_rva': hex(struct.unpack_from('<I', data, optional + 16)[0]),
            'image_base': hex(struct.unpack_from('<I', data, optional + 28)[0]),
            'matches_upstream_compact_size': len(data) == 5189632,
            'version': 'unverified; size alone cannot establish version',
        },
        'assets': {name: (root / name).stat().st_size if (root / name).is_file()
                   else None for name in assets},
        'scope': 'Presence and PE headers only; asset contents are not validated',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_directory', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.game_directory), indent=2))
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'Audit failed: {error}\n')


if __name__ == '__main__':
    main()
