"""Compile and run production upload code with offline fake clients."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run_test(root, name, production_source):
    sources = [root / f"test/host/{name}_test.cpp", root / production_source]
    with tempfile.TemporaryDirectory(prefix="edol-agent-tests-") as tmp:
        executable = Path(tmp) / (f"{name}_test.exe" if os.name == "nt" else f"{name}_test")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            installation = subprocess.check_output([
                str(vswhere), "-latest", "-products", "*", "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
            ], text=True).strip()
            vcvars = Path(installation) / "VC/Auxiliary/Build/vcvars64.bat"
            args = ["cl", "/nologo", "/std:c++14", "/EHsc", "/W4", "/WX",
                    f"/I{root / 'test/host/stubs'}", f"/I{root / 'src'}",
                    *map(str, sources), f"/Fe:{executable}"]
            batch = Path(tmp) / "build.cmd"
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                             + subprocess.list2cmdline(args) + "\nexit /b %errorlevel%\n")
            subprocess.run(["cmd", "/d", "/c", str(batch)], cwd=tmp, check=True)
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise RuntimeError("A native C++ compiler is required")
            subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            f"-I{root / 'test/host/stubs'}", f"-I{root / 'src'}",
                            *map(str, sources), "-o", str(executable)], cwd=tmp, check=True)
        subprocess.run([str(executable)], check=True)


def main():
    root = Path(__file__).resolve().parents[1]
    run_test(root, "upload", "src/http/HttpUploadClient.cpp")
    run_test(root, "agent", "src/agent/AgentService.cpp")


if __name__ == "__main__":
    main()
