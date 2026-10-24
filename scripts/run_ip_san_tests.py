"""Offline certificate verification against the exact audited Espressif mbedTLS source."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command, cwd=None):
    result = subprocess.run([str(value) for value in command], cwd=cwd, check=False, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError("Disposable fixture command failed: " + result.stderr[-2000:])


def fixtures(directory, openssl):
    # Disposable keys/certificates only. Never read or change the Stage 9 CA.
    (directory / "req.cnf").write_text("[req]\ndistinguished_name=dn\n[dn]\n")
    for name in ("ca", "foreign-ca"):
        run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "2",
             "-config", "req.cnf", "-subj", f"/CN=Offline {name}", "-addext", "basicConstraints=critical,CA:TRUE",
             "-addext", "keyUsage=critical,keyCertSign,cRLSign", "-keyout", f"{name}.key", "-out", f"{name}.pem"], directory)
    run([openssl, "req", "-new", "-newkey", "rsa:2048", "-nodes", "-config", "req.cnf",
         "-subj", "/CN=192.0.2.10", "-keyout", "leaf.key", "-out", "leaf.csr"], directory)
    (directory / "index").write_text("")
    (directory / "serial").write_text("01\n")
    (directory / "ca.cnf").write_text(
        "[ca]\ndefault_ca=fixture\n[fixture]\ndatabase=index\nnew_certs_dir=.\n"
        "certificate=ca.pem\nprivate_key=ca.key\nserial=serial\ndefault_md=sha256\n"
        "policy=names\nunique_subject=no\n[names]\ncommonName=supplied\n")
    sans = {"ip": "IP:192.0.2.10", "wrong-ip": "IP:192.0.2.11", "cn-only": None,
            "dns-ip": "DNS:192.0.2.10", "dns": "DNS:core.example.test",
            "mixed": "DNS:unrelated.example.test,IP:2001:db8::1,IP:192.0.2.10",
            "malformed": "DER:30:05:87:03:c0:00:02", "expired": "IP:192.0.2.10", "future": "IP:192.0.2.10"}
    for name, san in sans.items():
        extensions = "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n"
        if san is not None:
            extensions += f"subjectAltName={san}\n"
        (directory / "leaf.ext").write_text(extensions)
        arguments = [openssl, "ca", "-batch", "-notext", "-config", "ca.cnf", "-extfile", "leaf.ext",
                     "-in", "leaf.csr", "-out", f"{name}.pem"]
        if name == "expired":
            arguments += ["-startdate", "20000101000000Z", "-enddate", "20010101000000Z"]
        elif name == "future":
            arguments += ["-startdate", "20900101000000Z", "-enddate", "20910101000000Z"]
        else:
            arguments += ["-days", "1"]
        run(arguments, directory)
    run([openssl, "x509", "-in", "ip.pem", "-badsig", "-out", "bad-signature.pem"], directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mbedtls-source", required=True, type=Path)
    parser.add_argument("--openssl", default=shutil.which("openssl"))
    args = parser.parse_args()
    source = args.mbedtls_source.resolve()
    expected = {"x509_crt.c": "775c25456678fdc32960761ef6d18cc18e32fe283fe0c578bc3ceeb77b9f38a5",
                "ssl_tls.c": "5b3a413bc4c14a4c3d6df543b391034f661c5e4e734886da0e62efab5acb4c2a"}
    for name, digest in expected.items():
        if hashlib.sha256((source / "library" / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError("Source differs from audited Espressif 2b8e772 vendor revision")
    if not args.openssl:
        raise RuntimeError("Supply --openssl with an installed OpenSSL executable")
    root = Path(__file__).resolve().parents[1]
    build = root / ".pio/ip-san-native-build"
    build.mkdir(parents=True, exist_ok=True)
    fixture = Path(tempfile.mkdtemp(prefix="fixtures-", dir=build))
    fixtures(fixture, args.openssl)
    configure = ["cmake", "-S", str(root / "test/ip_san"), "-B", str(build),
                 f"-DMBEDTLS_SOURCE={source}", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5", "-DCMAKE_BUILD_TYPE=Debug"]
    compile_command = ["cmake", "--build", str(build), "--config", "Debug", "--parallel", "4"]
    if os.name == "nt":
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = Path(subprocess.check_output([str(vswhere), "-latest", "-products", "*",
                            "-property", "installationPath"], text=True).strip())
        cmake_tools = installation / "Common7/IDE/CommonExtensions/Microsoft/CMake"
        configure[0] = str(cmake_tools / "CMake/bin/cmake.exe")
        compile_command[0] = configure[0]
        configure += ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={cmake_tools / 'Ninja/ninja.exe'}"]
        batch = build / "build.cmd"
        batch.write_text(f'@echo off\ncall "{installation / "VC/Auxiliary/Build/vcvars64.bat"}" >nul\n'
                         'if errorlevel 1 exit /b 1\n'
                         + subprocess.list2cmdline(configure) + '\nif errorlevel 1 exit /b 1\n'
                         + subprocess.list2cmdline(compile_command) + '\nexit /b %errorlevel%\n')
        subprocess.run(["cmd", "/d", "/c", str(batch)], check=True)
    else:
        subprocess.run(configure, check=True)
        subprocess.run(compile_command, check=True)
    executable = build / ("ip_san_test.exe" if os.name == "nt" else "ip_san_test")
    subprocess.run([str(executable), str(fixture)], check=True)
    print("Fixtures retained only under ignored .pio; no Stage 9 endpoint or trust material used.")


if __name__ == "__main__":
    main()
