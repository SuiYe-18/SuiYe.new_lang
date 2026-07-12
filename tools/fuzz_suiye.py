import random
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUIYE = ROOT / "bin" / ("suiye.exe" if sys.platform.startswith("win") else "suiye")
TMP = ROOT / "build" / "fuzz"

WORDS = ["alpha", "beta", "gamma", "SuiYe_TR", "42", "true", "false"]
COMMANDS = ["println", "let", "set", "array", "push", "map", "put", "get", "unset", "now", "random", "typeof", "len", "upper", "lower"]

def rand_line():
    cmd = random.choice(COMMANDS)
    if cmd == "println":
        return f'println "{random.choice(WORDS)}"'
    if cmd in {"let", "set"}:
        return f"{cmd} v{random.randint(0, 9)} {random.randint(0, 999)}"
    if cmd == "array":
        return f"array a{random.randint(0, 4)} 1 2 3"
    if cmd == "push":
        return f"push a{random.randint(0, 4)} {random.randint(0, 99)}"
    if cmd == "map":
        return f"map m{random.randint(0, 4)}"
    if cmd == "put":
        return f'put m{random.randint(0, 4)} name "{random.choice(WORDS)}"'
    if cmd == "get":
        return f"get m{random.randint(0, 4)} name out{random.randint(0, 4)}"
    if cmd == "unset":
        return f"unset v{random.randint(0, 9)}"
    if cmd in {"now", "random"}:
        return f"{cmd} out{random.randint(0, 9)}"
    if cmd in {"typeof", "len", "upper", "lower"}:
        return f'{cmd} "{random.choice(WORDS)}" out{random.randint(0, 9)}'
    return "println \"fallback\""

def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    TMP.mkdir(parents=True, exist_ok=True)
    for i in range(count):
        script = "\n".join(rand_line() for _ in range(random.randint(5, 40))) + "\n"
        path = TMP / f"case_{i:05d}.sye"
        path.write_text(script, encoding="utf-8")
        proc = subprocess.run([str(SUIYE), "--check", str(path)], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if proc.returncode not in (0, 1):
            print(f"crash-like exit {proc.returncode} at {path}")
            print(proc.stdout)
            print(proc.stderr)
            return proc.returncode
    print(f"fuzz-ok cases={count}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
