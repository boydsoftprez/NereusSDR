"""The shared upstream header covers only named Setup JSON resources."""
import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
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


class CatDataHeaderTest(unittest.TestCase):
    def test_allowlisted_cat_data_requires_named_sidecar_and_markers(self):
        with TemporaryDirectory() as root, patch.object(module, "REPO", Path(root)):
            for relative in ("resources/cat/CommandContracts.json", "tests/data/cat/requests.json", "tests/data/cat/compatibility.csv"):
                path = Path(root) / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("data")
                sidecar = path.parent / "HEADERS.md"
                if sidecar.exists():
                    sidecar.unlink()
                markers = module.MARKERS_BY_KIND["thetis"]
                self.assertEqual(module.check_required_markers(path, markers), markers)
                full = " ".join(markers)
                sidecar.write_text(full)
                self.assertEqual(module.check_required_markers(path, markers), markers)
                sidecar.write_text(full + f" `{path.name}`")
                self.assertEqual(module.check_required_markers(path, markers), [])
                sidecar.write_text(full.replace("Copyright (C)", "") + f" `{path.name}`")
                self.assertEqual(module.check_required_markers(path, markers), ["Copyright (C)"])
                other = path.with_name("not-allowlisted.json")
                other.write_text("{}")
                sidecar.write_text(full + f" `{other.name}`")
                self.assertEqual(module.check_required_markers(other, markers), markers)

    def test_exact_xml_requires_project_license_and_no_header_statement(self):
        with TemporaryDirectory() as root, patch.object(module, "REPO", Path(root)):
            path = Path(root) / "resources/cat/CATStructs.xml"
            path.parent.mkdir(parents=True)
            path.write_text("<catstructs/>")
            sidecar = path.parent / "HEADERS.md"
            markers = module.MARKERS_BY_KIND["thetis"]
            full = " ".join(m for m in markers if m != "Copyright (C)")
            no_header = "Upstream source has no top-of-file GPL header — project-level LICENSE applies"
            sidecar.write_text(full + " `CATStructs.xml` " + no_header)
            self.assertEqual(module.check_required_markers(path, markers), [])
            for marker in [*filter(lambda m: m != "Copyright (C)", markers), no_header]:
                sidecar.write_text((full + " `CATStructs.xml` " + no_header).replace(marker, ""))
                self.assertIn(marker, module.check_required_markers(path, markers))
            sidecar.write_text(full + " `another.xml` " + no_header)
            self.assertTrue(module.check_required_markers(path, markers))
            sidecar.unlink()
            self.assertTrue(module.check_required_markers(path, markers))


if __name__ == "__main__":
    unittest.main()
