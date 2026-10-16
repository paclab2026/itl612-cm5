#!/usr/bin/env python3
"""Create a standalone source archive with normalized archive ownership."""
import gzip
import re
import sys
import tarfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
version = re.search(r'project\(ITL612CM5 VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
out = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build-deb'
out.mkdir(parents=True, exist_ok=True)
archive = out / f'itl612-cm5-{version}-source.tar.gz'
files = []
for directory in ('src', 'shaders', 'assets', 'tests', 'packaging'):
    files.extend(p for p in (root / directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc')
files.extend(root / name for name in ('CMakeLists.txt', 'README.md', 'README.en.md', 'install.sh', 'install-desktop.sh', 'itl612-start', 'itl612-cm5.desktop', '70-itl612-mars.rules'))
with archive.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as gz, tarfile.open(fileobj=gz, mode='w') as tf:
    for path in sorted(files):
        if path.is_symlink():
            raise RuntimeError('Source files must not be symbolic links')
        info = tf.gettarinfo(str(path), arcname=f'itl612-cm5-{version}-source/{path.relative_to(root)}')
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = 'root'
        info.mode = 0o755 if path.suffix == '.sh' or path.name in ('itl612-start', 'postinst', 'postrm') else 0o644
        with path.open('rb') as data:
            tf.addfile(info, data)
print(archive.name)
