#!/usr/bin/env python3
"""Fail on German in code and docs: this repo is English only.

  tools/check_language.py              # all tracked text files
  tools/check_language.py --files a b  # only these files (pre-commit, editor hook)
  --root DIR                           # resolve paths against DIR (pre-commit checks a copy of the index)

Signals: umlauts / sharp s, and a short list of German words that do not exist in English.
Exit 1 with file:line on a hit.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if "--root" in sys.argv:
    ROOT = Path(sys.argv[sys.argv.index("--root") + 1]).resolve()
EXTS = {".yaml", ".yml", ".md", ".h", ".py", ".sh", ".json", ".txt", ""}
WORDS = (
    "und nicht wird werden wurde eine einen einem kein keine noch auch oder wenn dann schon "
    "kaffee fertig heizen einschalten ausschalten schalter bereit warten ueber fuer"
).split()
PATTERN = re.compile(r"[äöüÄÖÜß]|\b(" + "|".join(WORDS) + r")\b", re.IGNORECASE)
SKIP = {"tools/check_language.py"}


def files_from_args() -> list[Path]:
    if "--files" in sys.argv:
        args = sys.argv[sys.argv.index("--files") + 1:]
        if "--root" in args:
            args = args[:args.index("--root")]
        return [(ROOT / f).resolve() for f in args]
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                         capture_output=True, text=True, check=True).stdout
    return [ROOT / f for f in out.splitlines()]


def main() -> int:
    hits = []
    for path in files_from_args():
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            continue  # outside the repo
        if rel in SKIP or path.suffix not in EXTS or not path.is_file():
            continue
        for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = PATTERN.search(line)
            if m:
                hits.append(f"{rel}:{n}: '{m.group(0)}' -> {line.strip()[:100]}")
    if hits:
        print("German found (this repo is English only):", *hits, sep="\n  ", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
