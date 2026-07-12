import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
PKG = ROOT / "suiye.toml"
PACKAGES = ROOT / "packages"
SUIYE = Path(__file__).resolve().parents[1] / "bin" / ("suiye.exe" if sys.platform.startswith("win") else "suiye")

def init(args):
    if PKG.exists() and not args.force:
        print("suiye.toml already exists")
        return 1
    PKG.write_text('[package]\nname = "suiye-app"\nversion = "0.1.0"\ncreator = "SuiYe_TR"\n\n[dependencies]\n', encoding="utf-8")
    PACKAGES.mkdir(exist_ok=True)
    print("package initialized")
    return 0

def install(args):
    PACKAGES.mkdir(exist_ok=True)
    src = Path(args.source).resolve()
    if not src.exists():
        print(f"source not found: {src}")
        return 1
    dst = PACKAGES / (args.name or src.stem)
    if dst.exists():
        shutil.rmtree(dst) if dst.is_dir() else dst.unlink()
    if src.is_dir():
        shutil.copytree(src, dst)
    else:
        dst.mkdir()
        shutil.copy2(src, dst / src.name)
    print(f"installed {dst.name}")
    return 0

def list_packages(_args):
    PACKAGES.mkdir(exist_ok=True)
    for p in sorted(PACKAGES.iterdir()):
        print(p.name)
    return 0

def run(args):
    return subprocess.call([str(SUIYE), "--run", args.script], cwd=ROOT)

def main():
    p = argparse.ArgumentParser(prog="suiye-pkg")
    sub = p.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("init")
    a.add_argument("--force", action="store_true")
    a.set_defaults(func=init)
    a = sub.add_parser("install")
    a.add_argument("source")
    a.add_argument("--name")
    a.set_defaults(func=install)
    a = sub.add_parser("list")
    a.set_defaults(func=list_packages)
    a = sub.add_parser("run")
    a.add_argument("script")
    a.set_defaults(func=run)
    args = p.parse_args()
    return args.func(args)

if __name__ == "__main__":
    raise SystemExit(main())
