# no-port-check: NereusSDR-original.
"""Puts the server package and the test helpers on the import path."""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "server"))
sys.path.insert(0, str(HERE))
