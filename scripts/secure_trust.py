"""Generate a build-local public CA header only for the explicit secure environment."""

import json
import os
from pathlib import Path
import ssl
import time


def write_trust_header(environment):
    if environment.subst("$PIOENV") not in ("lolin_d32_secure", "lolin_d32_secure_smoke"):
        return
    configured = os.environ.get("EDOL_AGENT_ROOT_CA_FILE")
    if not configured:
        raise RuntimeError("Secure agent build requires EDOL_AGENT_ROOT_CA_FILE")
    source = Path(configured)
    if source.stat().st_size > 32768:
        raise RuntimeError("Agent CA bundle exceeds the build limit")
    certificate = source.read_text(encoding="ascii")
    if "PRIVATE KEY" in certificate:
        raise RuntimeError("Agent trust bundle must contain certificates only")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_verify_locations(cadata=certificate)
    output = Path(environment.subst("$BUILD_DIR")) / "generated"
    output.mkdir(parents=True, exist_ok=True)
    (output / "edol_agent_trust.h").write_text(
        "#pragma once\nstatic const char EDOL_AGENT_ROOT_CA[] = "
        + json.dumps(certificate, ensure_ascii=True)
        + ";\nstatic constexpr long EDOL_AGENT_BUILD_EPOCH = " + str(int(time.time())) + ";\n",
        encoding="ascii",
    )
    environment.Append(CPPPATH=[str(output)])


if "Import" in globals():
    Import("env")
    write_trust_header(env)
