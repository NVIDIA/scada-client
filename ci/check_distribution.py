#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
"""Check the source distribution without a CUDA compiler, GPU or server."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def main():
    errors = []
    count = 0
    for path in sorted(ROOT.rglob('*')):
        relative = path.relative_to(ROOT)
        if any(part in {'.git', '__pycache__', '.cache', '_deps'} or part.startswith('build')
               for part in relative.parts[:-1]):
            continue
        if path.is_symlink():
            errors.append(f'{relative}: unexpected symlink')
            continue
        if not path.is_file():
            continue
        count += 1
        payload = path.read_bytes()
        if path.name == 'LICENSE':
            if b'Apache License' not in payload or b'Version 2.0' not in payload:
                errors.append(f'{relative}: missing Apache license text')
            continue
        if relative.as_posix() == 'LICENSES/CC-BY-4.0.txt':
            if b'Creative Commons Attribution 4.0' not in payload:
                errors.append(f'{relative}: missing Creative Commons license text')
            continue
        sidecar = path.with_name(path.name + '.license')
        notice = sidecar.read_text() if sidecar.is_file() else payload[:4096].decode('utf-8', errors='replace')
        if 'SPDX-FileCopyrightText:' not in notice or 'SPDX-License-Identifier:' not in notice:
            errors.append(f'{relative}: missing copyright/license metadata')
        if path.suffix == '.png' and not payload.startswith(b'\x89PNG\r\n\x1a\n'):
            errors.append(f'{relative}: expected a real PNG image')
        if b'\0' in payload:
            continue
        text = payload.decode('utf-8')
        forbidden = ('LicenseRef-' + 'NvidiaProprietary', 'its affiliates is strictly ' + 'prohibited.')
        if any(item in text for item in forbidden):
            errors.append(f'{relative}: contains proprietary license restrictions')
        if re.search(r'(?:gitlab[^/\s]*|confluence|nvbugs|urm)' + r'\.nvidia\.com', text, re.IGNORECASE):
            errors.append(f'{relative}: contains an internal infrastructure reference')
        if text.startswith('version https://git-lfs.github.com/spec/v1\n'):
            errors.append(f'{relative}: unresolved Git LFS pointer')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'Checked copyright, license and distribution content for {count} files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
