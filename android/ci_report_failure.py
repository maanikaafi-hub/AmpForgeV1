#!/usr/bin/env python3
"""Emit a concise GitHub annotation with the tail of a failed Gradle log."""
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: ci_report_failure.py LOG_PATH TITLE")
log_path, title = sys.argv[1:]
message = Path(log_path).read_text(errors="replace")[-2500:]
message = message.replace("%", "%25").replace("\r", "").replace("\n", " | ")
print(f"::error title={title}::{message}")
