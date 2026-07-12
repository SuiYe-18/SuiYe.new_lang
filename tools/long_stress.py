import argparse
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUIYE = ROOT / "bin" / ("suiye.exe" if sys.platform.startswith("win") else "suiye")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=int, default=300)
    parser.add_argument("--script", default="tests/core_part3_ast_runtime.sye")
    args = parser.parse_args()
    deadline = time.time() + args.seconds
    runs = 0
    while time.time() < deadline:
        proc = subprocess.run([str(SUIYE), "--ast-run", args.script], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if proc.returncode != 0:
            print(proc.stderr)
            return proc.returncode
        runs += 1
    print(f"long-stress-ok seconds={args.seconds} runs={runs}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
