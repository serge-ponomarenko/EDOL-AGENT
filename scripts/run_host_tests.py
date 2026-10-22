"""Compile and run production upload code with offline fake clients."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run_test(root, name, production_source, secure=False):
    sources = [root / f"test/host/{name}_test.cpp", root / production_source]
    extra_includes = []
    if name in ("enrollment", "mqtt_enrollment"):
        library = root / ".pio/libdeps/lolin_d32/ArduinoJson/src"
        if not library.is_dir():
            raise RuntimeError("Build lolin_d32 first to resolve the real ArduinoJson parser for enrollment tests")
        extra_includes.append(library)
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
                    *[f"/I{path}" for path in extra_includes],
                    *map(str, sources), f"/Fe:{executable}"]
            if secure:
                args.append("/DEDOL_AGENT_SECURE_BUILD=1")
            batch = Path(tmp) / "build.cmd"
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                             + subprocess.list2cmdline(args) + "\nexit /b %errorlevel%\n")
            subprocess.run(["cmd", "/d", "/c", str(batch)], cwd=tmp, check=True)
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise RuntimeError("A native C++ compiler is required")
            subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            *(["-DEDOL_AGENT_SECURE_BUILD=1"] if secure else []),
                            f"-I{root / 'test/host/stubs'}", f"-I{root / 'src'}",
                            *[f"-I{path}" for path in extra_includes],
                            *map(str, sources), "-o", str(executable)], cwd=tmp, check=True)
        subprocess.run([str(executable)], check=True)


def main():
    root = Path(__file__).resolve().parents[1]
    run_test(root, "upload", "src/http/HttpUploadClient.cpp")
    run_test(root, "agent", "src/agent/AgentService.cpp")
    run_test(root, "credential", "src/security/AgentCredentialStore.cpp")
    run_test(root, "secure_upload", "src/http/HttpUploadClient.cpp", secure=True)
    run_test(root, "enrollment", "src/security/AgentEnrollmentClient.cpp", secure=True)
    run_test(root, "mqtt_enrollment", "src/security/AgentMqttEnrollmentClient.cpp", secure=True)
    run_test(root, "mqtt", "src/mqtt/EdolMqttClient.cpp")
    run_test(root, "mqtt", "src/mqtt/EdolMqttClient.cpp", secure=True)


if __name__ == "__main__":
    main()
