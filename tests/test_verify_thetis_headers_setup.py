"""The shared upstream header covers only named Setup JSON resources."""
import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/verify-thetis-headers.py"
spec = importlib.util.spec_from_file_location("verify_thetis_headers", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SetupJsonHeaderTest(unittest.TestCase):
    def test_named_json_uses_shared_header(self):
        with TemporaryDirectory() as root:
            setup = Path(root) / "resources/setup"
            setup.mkdir(parents=True)
            (setup / "HEADERS.md").write_text(
                "Ported from Thetis. Copyright (C) 2026. "
                "General Public License. Modification history (NereusSDR). "
                "Covered: `general.json`."
            )
            named = setup / "general.json"
            unnamed = setup / "other.json"
            named.write_text("{}")
            unnamed.write_text("{}")
            markers = module.MARKERS_BY_KIND["thetis"]
            self.assertEqual(module.check_required_markers(named, markers), [])
            self.assertEqual(module.check_required_markers(unnamed, markers), markers)
            (setup / "HEADERS.md").unlink()
            self.assertEqual(module.check_required_markers(named, markers), markers)


if __name__ == "__main__":
    unittest.main()
