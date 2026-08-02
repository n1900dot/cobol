# Four compiler fixes

Branch: `fix/compiler-fixes`

## What is fixed

1. **ACCEPT FROM TIME** – removed broken TZ offset (~9 min drift); uses `clock_gettime` seconds directly
2. **ACCEPT FROM DATE** – 4-digit year (YYYYMMDD, 8 bytes) instead of 2-digit
3. **EVALUATE** – works on record subordinates and THRU ranges via `elementarySize()`
4. **REDEFINES** – always copies target metadata and emits `_len` symbols
5. **READ RECORD KEY IS** – flexible clause order (KEY IS / RECORD KEY IS / INTO / NEXT RECORD)

## Apply locally

```bash
# On a clean tree matching main (commit 835de6ce or later clean upload):
patch -p1 < patches/four-fixes.patch
mkdir -p build && cd build && cmake ../src && make
```

## Apply via GitHub Actions

1. Open **Actions** → **Apply four compiler fixes**
2. Run workflow on branch `fix/compiler-fixes`
3. Workflow patches `src/parser.cpp` and `src/code_generator.h` and commits
4. Merge the branch into main

Or apply the patch yourself and merge:

```bash
git checkout fix/compiler-fixes
patch -p1 < patches/four-fixes.patch
git add src/parser.cpp src/code_generator.h
git commit -m "Apply four compiler fixes"
git push
# then merge PR
```
