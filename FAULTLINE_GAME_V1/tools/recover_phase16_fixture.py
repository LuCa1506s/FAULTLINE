"""Reproduce the authentic uncommitted Phase 16 writer in an isolated repo-local tree.

Frozen source patch is against Phase 15 e60833b, not the evolving current writer.
This tool never reads, changes, truncates or fabricates save bytes.
"""
from pathlib import Path
import subprocess
import hashlib
import sys

root = Path(__file__).resolve().parents[1]
dest = (root / (sys.argv[1] if len(sys.argv)>1 else 'build/phase16-writer-reproduction')).resolve()
assert dest.is_relative_to(root / 'build'), 'Recovery output must stay under repository build/'
assert not dest.exists(), 'Never overwrite an existing recovery tree'
dest.mkdir(parents=True)
commit = 'e60833b'
for name in subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', commit], cwd=root, text=True).splitlines():
    if name.startswith(('src/', 'cmake/', 'tools/')) or name == 'CMakeLists.txt':
        p = dest / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(subprocess.check_output(['git', 'show', commit + ':' + name], cwd=root))
subprocess.run(['git', 'apply', '--directory=' + dest.relative_to(root).as_posix(),
                'tests/fixtures/phase16_writer.patch'], cwd=root, check=True)
manifest = []
for p in sorted(dest.rglob('*')):
    if p.is_file():
        manifest.append(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + str(p.relative_to(dest)))
(dest / 'SOURCE_SHA256.txt').write_text('\n'.join(manifest) + '\n')
print(dest)
