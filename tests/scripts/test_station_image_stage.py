"""Offline lint for the station card-image inputs; never boots or enables services."""

from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
STATION = ROOT / "packaging/station-image"
WORKFLOW = ROOT / ".github/workflows/station-image.yml"


def read(relative: str) -> str:
    return (STATION / relative).read_text()


def test_shell_scripts_parse_without_execution():
    for path in STATION.rglob("*.sh"):
        subprocess.run(["bash", "-n", str(path)], check=True)


def test_stage_inherits_lite_rootfs_and_installs_only_matching_package():
    assert "copy_previous" in read("pi-gen/stage-nereus/prerun.sh")
    assert read("pi-gen/stage-nereus/00-install/00-packages").splitlines() == ["avahi-daemon"]
    stage = read("pi-gen/stage-nereus/00-install/00-run.sh")
    assert "${ROOTFS_DIR}/tmp/nereus-install" in stage
    assert "on_chroot" in stage
    assert "rm -rf \"${ROOTFS_DIR}/tmp/nereus-install\"" in stage
    installer = read("common/install-station.sh")
    for required in (
        "VERSION_CODENAME:-}\" = trixie",
        "--print-architecture)",
        "dpkg-deb --field \"$deb\" Package",
        "dpkg-deb --field \"$deb\" Architecture",
        "apt-get install -y --no-install-recommends \"$deb\" avahi-daemon",
        "DynamicUser=yes",
        "systemctl enable nereus-firstboot.service nereusd.service avahi-daemon.service",
    ):
        assert required in installer
    assert not re.search(r"--force-(?:depends|architecture)|--allow-unauthenticated", installer)


def test_first_boot_and_config_are_clean():
    assets = [p for p in STATION.rglob("*") if p.is_file()]
    forbidden_names = ("identity", "private", "token", "devices.json", ".pem", ".key")
    assert all(not any(word in p.name.lower() for word in forbidden_names) for p in assets)
    installer = read("common/install-station.sh")
    assert "nereusd.conf.sample /etc/nereusd.conf" in installer
    sample = (ROOT / "packaging/nereusd.conf.sample").read_text()
    assert re.search(r"^radio_mac\s*=\s*$", sample, re.MULTILINE)
    assert "station-identity" not in installer
    firstboot = read("common/nereus-firstboot.sh")
    assert "first-boot-pending" in firstboot
    assert "hostnamectl set-hostname nereus-station" in firstboot
    assert "rm -f \"$marker\"" in firstboot
    assert "pairing" not in firstboot.lower()
    assert "token" not in firstboot.lower()
    unit = read("common/nereus-firstboot.service")
    assert "Before=nereusd.service" in unit
    assert "ConditionPathExists=/var/lib/nereus-station/first-boot-pending" in unit


def test_armbian_contract_and_workflow_provenance():
    armbian = read("armbian/customize-image.sh")
    assert "trixie" in armbian and "rock-5c" in armbian
    assert "/tmp/overlay/nereus-station" in armbian
    assert "install-station.sh" in armbian
    workflow = WORKFLOW.read_text()
    assert "workflow_dispatch:" in workflow
    assert "ubuntu-24.04-arm" in workflow
    assert "74d08a337bd29da289b9aedbe5b48c79fb2e5a03" in workflow
    assert "RELEASE=trixie" in workflow
    assert "ENABLE_CLOUD_INIT=0" in workflow
    assert "04-cloud-init/SKIP" in workflow
    assert "ENABLE_SSH=0" in workflow
    assert "DEPLOY_COMPRESSION=xz" in workflow
    assert "source_sha" in workflow and "image_sha256" in workflow
    assert '"dfnr": dfnr' in workflow
    assert "rust:1.94.1-trixie" in workflow
    assert "_arm64_trixie.deb" in workflow
    assert "upload-artifact@v4" in workflow
    assert "release.yml" not in workflow
    builder = read("build-trixie-deb.sh")
    assert "-march=armv8-a" in builder
    assert "-DENABLE_DFNR=ON" in builder
    assert "build-dfnr-source.sh" in builder
    assert "DeepFilterFilter.cpp" in builder and "-DHAVE_DFNR" in builder
    assert "DeepFilterNet3_onnx.tar.gz" in builder
    assert "-march=native" not in builder and "-mcpu=native" not in builder


def test_dfnr_source_is_locked_and_model_is_packaged():
    dfnr = read("build-dfnr-source.sh")
    assert "d375b2d8309e0935d165700c91da9de862a99c31" in dfnr
    assert "cargo install cargo-c --version '0.10.21+cargo-0.95.0' --locked" in dfnr
    assert "cargo cbuild --locked --release" in dfnr
    assert "RUSTFLAGS='-C target-cpu=generic'" in dfnr
    assert "-march=armv8-a" in dfnr
    assert "DeepFilterNet-Cargo.lock" in dfnr
    assert "setup-deepfilter.sh" not in dfnr
    lock = read("DeepFilterNet-Cargo.lock")
    assert 'name = "deep_filter"' in lock
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert 'install(FILES "${DFNR_MODEL}"\n                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/NereusSDR/models/dfnet3"\n                COMPONENT nereusd\n                EXCLUDE_FROM_ALL)' in cmake
    verifier = read("verify-package.sh")
    assert "usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz" in verifier


def test_daemon_component_installs_dfnr_model(tmp_path):
    # Exercise the repository's actual install rules with a small stand-in
    # model, so component selection is checked without building the radio app.
    cmake = (ROOT / "CMakeLists.txt").read_text()
    rules = re.findall(
        r'install\(FILES "\$\{DFNR_MODEL\}"\s+'
        r'DESTINATION "\$\{CMAKE_INSTALL_DATAROOTDIR\}/NereusSDR/models/dfnet3"'
        r'[^)]*\)', cmake)
    assert rules
    source = tmp_path / "source"
    source.mkdir()
    (source / "model.tar.gz").write_bytes(b"station-model-test")
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.20)\n'
        'project(StationModelInstall NONE)\n'
        'set(CMAKE_INSTALL_DATAROOTDIR share)\n'
        'set(DFNR_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/model.tar.gz")\n'
        + "\n".join(rules) + "\n")
    build = tmp_path / "build"
    subprocess.run(["cmake", "-S", str(source), "-B", str(build)], check=True)
    for component in ("nereusd", None):
        prefix = tmp_path / (component or "desktop")
        command = ["cmake", "--install", str(build), "--prefix", str(prefix)]
        if component:
            command += ["--component", component]
        subprocess.run(command, check=True)
        model = prefix / "share/NereusSDR/models/dfnet3/model.tar.gz"
        assert model.read_bytes() == b"station-model-test"
