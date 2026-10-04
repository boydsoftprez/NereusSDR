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
inventory_spec = importlib.util.spec_from_file_location("compliance_inventory", SCRIPT.with_name("compliance-inventory.py"))
inventory = importlib.util.module_from_spec(inventory_spec)
inventory_spec.loader.exec_module(inventory)


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


class CatTcpNoticeTest(unittest.TestCase):
    source = "Project Files/Source/Console/CAT/TCPIPcatServer.cs"
    notice = ("//=================================================================\n"
              "// MW0LGE 2022\n"
              "//=================================================================\n\n"
              "// inspiration from https://www.codeproject.com/Articles/5733/A-TCP-IP-Server-written-in-C\n"
              "//\n")
    attribution = ("// Ported from Thetis Project Files/Source/Console/CAT/TCPIPcatServer.cs\n"
                   "// Upstream source has an author/inspiration notice; project-level GNU General Public License applies.\n"
                   "// Modification history (NereusSDR):\n")

    def test_exact_tcp_notice_and_source_pass(self):
        with TemporaryDirectory() as root, patch.object(module, "REPO", Path(root)):
            path = Path(root) / "src/core/cat/CatTcpTransport.cpp"
            path.parent.mkdir(parents=True)
            path.write_text(self.notice + self.attribution)
            self.assertEqual(module.check_required_markers(path, module.MARKERS_BY_KIND["thetis"], self.source), [])

    def test_inventory_uses_same_exact_notice_rule(self):
        with TemporaryDirectory() as root, patch.object(inventory, "REPO", Path(root)):
            relative = "src/core/cat/CatSession.cpp"
            path = Path(root) / relative
            path.parent.mkdir(parents=True)
            path.write_text(self.notice + self.attribution)
            self.assertEqual(inventory._verify_markers(relative, "thetis-port"), [])
            path.write_text(self.notice.replace("MW0LGE 2022", "MW0LGE 2023") + self.attribution)
            self.assertTrue(inventory._verify_markers(relative, "thetis-port"))
            path.write_text(self.notice + self.attribution)
            with patch.object(inventory, "THETIS_SOURCE_CELLS", {relative: self.source + "; console.cs"}):
                self.assertTrue(inventory._verify_markers(relative, "thetis-port"))

    def test_missing_mutated_or_wrong_source_notice_fails(self):
        with TemporaryDirectory() as root, patch.object(module, "REPO", Path(root)):
            path = Path(root) / "src/core/cat/CatTcpTransport.cpp"
            path.parent.mkdir(parents=True)
            for text in (self.attribution,
                         self.notice.replace("MW0LGE 2022", "MW0LGE 2023") + self.attribution,
                         self.notice + self.attribution.replace(self.source, "CAT/TCPIPcatServer.cs"),
                         self.notice + self.attribution.replace("project-level GNU General Public License applies.", ""),
                         self.notice + self.attribution + "// Ported from Thetis console.cs\n"):
                path.write_text(text)
                self.assertTrue(module.check_required_markers(path, module.MARKERS_BY_KIND["thetis"]))
            path.write_text(self.notice + self.attribution)
            self.assertTrue(module.check_required_markers(path, module.MARKERS_BY_KIND["thetis"], self.source + "; console.cs"))
            other = path.with_name("Other.cpp")
            other.write_text(self.notice + self.attribution)
            self.assertTrue(module.check_required_markers(other, module.MARKERS_BY_KIND["thetis"]))


if __name__ == "__main__":
    unittest.main()
