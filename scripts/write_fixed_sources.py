#!/usr/bin/env python3
"""Decode and write the fixed MiniCOBOL sources."""
from pathlib import Path
import base64, gzip

# Content loaded from companion files to keep this script small.
# Prefer: python3 scripts/apply_from_payload.py
print('Use scripts/apply_from_payload.py or run the GitHub Action apply-fixes')
