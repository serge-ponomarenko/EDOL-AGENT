"""Offline build-hook checks; generated CA fixtures are public system trust roots."""

import importlib.util
import json
import os
from pathlib import Path
import ssl
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "secure_trust", Path(__file__).resolve().parents[1] / "scripts/secure_trust.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FakeEnvironment:
    def __init__(self, directory, secure=True, smoke=False):
        self.directory = directory
        self.secure = secure
        self.smoke = smoke
        self.includes = []

    def subst(self, name):
        if name == "$PIOENV":
            return "lolin_d32_secure_smoke" if self.smoke else ("lolin_d32_secure" if self.secure else "lolin_d32")
        return str(self.directory)

    def Append(self, CPPPATH):
        self.includes.extend(CPPPATH)


class SecureTrustTest(unittest.TestCase):
    def test_home_needs_no_ca_but_secure_without_ca_fails(self):
        with tempfile.TemporaryDirectory() as temporary, patch.dict(os.environ, {}, clear=True):
            environment = FakeEnvironment(temporary, secure=False)
            MODULE.write_trust_header(environment)
            self.assertEqual(environment.includes, [])
            with self.assertRaisesRegex(RuntimeError, "EDOL_AGENT_ROOT_CA_FILE"):
                MODULE.write_trust_header(FakeEnvironment(temporary))
            with self.assertRaisesRegex(RuntimeError, "EDOL_AGENT_ROOT_CA_FILE"):
                MODULE.write_trust_header(FakeEnvironment(temporary, smoke=True))

    def test_public_ca_newlines_round_trip_through_generated_header(self):
        certificates = ssl.create_default_context().get_ca_certs(binary_form=True)
        self.assertTrue(certificates, "A public CA fixture must be available for this check")
        certificate = ssl.DER_cert_to_PEM_cert(certificates[0])
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / "fixture.pem"
            source.write_text(certificate, encoding="ascii")
            environment = FakeEnvironment(directory, smoke=True)
            with patch.dict(os.environ, {"EDOL_AGENT_ROOT_CA_FILE": str(source)}):
                MODULE.write_trust_header(environment)
            header = (directory / "generated/edol_agent_trust.h").read_text(encoding="ascii")
            encoded = header.split("EDOL_AGENT_ROOT_CA[] = ", 1)[1].split(";\n", 1)[0]
            self.assertEqual(json.loads(encoded), certificate)
            self.assertEqual(environment.includes, [str(directory / "generated")])

    def test_private_keys_oversized_and_invalid_inputs_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "fixture.pem"
            environment = FakeEnvironment(temporary)
            with patch.dict(os.environ, {"EDOL_AGENT_ROOT_CA_FILE": str(source)}):
                source.write_text("PRIVATE KEY", encoding="ascii")
                with self.assertRaisesRegex(RuntimeError, "certificates only"):
                    MODULE.write_trust_header(environment)
                source.write_text("x" * 32769, encoding="ascii")
                with self.assertRaisesRegex(RuntimeError, "build limit"):
                    MODULE.write_trust_header(environment)
                source.write_text("not a certificate", encoding="ascii")
                with self.assertRaises(ssl.SSLError):
                    MODULE.write_trust_header(environment)
            self.assertFalse((Path(temporary) / "generated").exists())


if __name__ == "__main__":
    unittest.main()
