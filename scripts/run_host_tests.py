"""Compile and run production upload code with offline fake clients."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

UPLOAD_SOURCE = "src/http/HttpUploadClient.cpp"
EDOL_MQTT_SOURCE = "src/mqtt/EdolMqttClient.cpp"


def test_inputs(root, name, production_source, secure):
    sources = [root / f"test/host/{name}_test.cpp"]
    if production_source:
        sources.append(root / production_source)
    if secure and production_source:
        sources.append(root / "src/security/SecureIpClient.cpp")
    if name == "bulk_task":
        sources.extend([root / "src/mqtt/BambuMqttClient.cpp", root / EDOL_MQTT_SOURCE, root / "src/mqtt/AgentEventPublisher.cpp"])
    if name == "tls_lifecycle":
        sources.extend([root / "src/security/SecureTransport.cpp", root / "src/security/AgentEnrollmentClient.cpp"])
    extra_includes = []
    if name == "bulk_task":
        extra_includes.append(root / "test/bulk/stubs")
    if name in ("ftps", "camera"):
        extra_includes.append(root / "test/ftps/stubs")
    if name in ("enrollment", "mqtt_enrollment", "tls_lifecycle", "bulk_task"):
        library = root / ".pio/libdeps/lolin_d32/ArduinoJson/src"
        if not library.is_dir():
            raise RuntimeError("Build lolin_d32 first to resolve the real ArduinoJson parser for enrollment tests")
        extra_includes.append(library)
    return sources, extra_includes


def run_test(root, name, production_source, secure=False, smoke=False):
    sources, extra_includes = test_inputs(root, name, production_source, secure)
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
                    *[f"/I{path}" for path in extra_includes],
                    f"/I{root / 'test/host/stubs'}", f"/I{root / 'src'}",
                    *map(str, sources), f"/Fe:{executable}"]
            if secure:
                args.append("/DEDOL_AGENT_SECURE_BUILD=1")
            if smoke:
                args.append("/DEDOL_AGENT_SMOKE_BUILD=1")
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
                            *(["-DEDOL_AGENT_SMOKE_BUILD=1"] if smoke else []),
                            *[f"-I{path}" for path in extra_includes],
                            f"-I{root / 'test/host/stubs'}", f"-I{root / 'src'}",
                            *map(str, sources), "-o", str(executable)], cwd=tmp, check=True)
        subprocess.run([str(executable)], check=True)


def main():
    root = Path(__file__).resolve().parents[1]
    run_test(root, "transfer_mailbox", None)
    run_test(root, "bulk_task", "src/agent/BulkTransferTask.cpp")
    run_test(root, "bulk_task", "src/agent/BulkTransferTask.cpp", secure=True)
    run_test(root, "camera", "src/camera/CameraClient.cpp")
    run_test(root, "upload", UPLOAD_SOURCE)
    run_test(root, "agent", "src/agent/AgentService.cpp")
    run_test(root, "agent", "src/agent/AgentService.cpp", secure=True)
    run_test(root, "credential", "src/security/AgentCredentialStore.cpp")
    run_test(root, "secure_upload", UPLOAD_SOURCE, secure=True)
    run_test(root, "enrollment", "src/security/AgentEnrollmentClient.cpp", secure=True)
    run_test(root, "mqtt_enrollment", "src/security/AgentMqttEnrollmentClient.cpp", secure=True)
    run_test(root, "mqtt", EDOL_MQTT_SOURCE)
    run_test(root, "mqtt", EDOL_MQTT_SOURCE, secure=True)
    run_test(root, "secure_transport", "src/security/SecureTransport.cpp", secure=True, smoke=True)
    run_test(root, "tls_lifecycle", UPLOAD_SOURCE, secure=True, smoke=True)
    run_test(root, "provisioning", None, secure=True)
    run_test(root, "ftps", "src/ftps/FtpsClient.cpp")


if __name__ == "__main__":
    main()
