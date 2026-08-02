#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p src
python3 - << 'PY'
from pathlib import Path
import base64, gzip
for name, dest in [('parser', 'src/parser.cpp'), ('codegen', 'src/code_generator.h')]:
    parts = sorted(Path(f'.fix-payload/{name}').glob('*'))
    b64 = ''.join(p.read_text().strip() for p in parts)
    data = gzip.decompress(base64.b64decode(b64))
    Path(dest).write_bytes(data)
    print(f'wrote {dest} ({len(data)} bytes)')
PY
head -2 src/parser.cpp
grep -c "Flexible clause\|elementarySize\|YYYYMMDD" src/parser.cpp src/code_generator.h
