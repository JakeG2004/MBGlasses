"""Release-contract unit fixtures only: no real installers, native checks, or network."""

from copy import deepcopy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from urllib.parse import parse_qs, urlsplit

import release

VERSION, TAG, COMMIT = "1.2.3", "v1.2.3", "a" * 40


class HTTPFixture:
    """Small stateful fixture at api.call's HTTP boundary, never a GitHub client."""

    def __init__(self, fault=None):
        self.release = None
        self.assets, self.payloads, self.calls = [], {}, []
        self.fault = fault
        self.list_reads = self.tag_reads = 0

    def call(self, method, path, data=None, binary=False):
        self.calls.append((method, path, data, binary))
        url = urlsplit(path)
        if method == "GET" and path == "/git/ref/tags/" + TAG:
            self.tag_reads += 1
            changed = self.fault == "tag" and self.tag_reads > 1
            return {"object": {"type": "commit", "sha": "b" * 40 if changed else COMMIT}}
        if method == "GET" and path == "/releases/tags/" + TAG:
            return deepcopy(self.release)
        if method == "POST" and path == "/releases/generate-notes":
            return {"body": "Generated notes"}
        if method == "POST" and path == "/releases":
            self.release = dict(data, id=1, upload_url="https://uploads.test/assets{?name,label}")
            return deepcopy(self.release)
        if method == "GET" and path == "/releases/1/assets?per_page=100":
            self.list_reads += 1
            assets = deepcopy(self.assets)
            if self.list_reads == 2:
                if self.fault == "missing":
                    assets.pop()
                elif self.fault in ("duplicate", "unexpected"):
                    assets.append(dict(assets[0], name="extra" if self.fault == "unexpected" else assets[0]["name"]))
            return assets
        if method == "POST" and url.netloc == "uploads.test":
            name = parse_qs(url.query)["name"][0]
            if self.fault == "upload" and name == "SHA256SUMS":
                raise release.ReleaseError("Fixture HTTP 503 during upload")
            payload = data.read_bytes()
            identifier = max(self.payloads, default=0) + 1
            asset = dict(id=identifier, name=name, size=len(payload), digest="sha256:" + hashlib.sha256(payload).hexdigest())
            if self.fault == "size":
                asset["size"] += 1
            self.assets.append(asset)
            self.payloads[identifier] = payload
            return dict(asset)
        if path.startswith("/releases/assets/"):
            identifier = int(url.path.rsplit("/", 1)[1])
            if method == "DELETE":
                self.assets = [asset for asset in self.assets if asset["id"] != identifier]
                return None
            if method == "GET" and binary:
                asset = next(asset for asset in self.assets if asset["id"] == identifier)
                return b"corrupted" if self.fault == "corrupt-" + asset["name"] else self.payloads[identifier]
        if method == "PATCH" and path == "/releases/1":
            self.release.update(data)
            return deepcopy(self.release)
        raise AssertionError(f"Unexpected fixture HTTP call: {method} {path}")


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(dir=Path(__file__).parent)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def bundle(self, signed=False):
        self.config = release.load_config()
        self.targets = release.targets(self.config)
        directory = Path(tempfile.mkdtemp(dir=self.root))
        for target in self.targets:
            package = directory / release.artifact_name(target, VERSION, signed)
            payload = ("Unit contract fixture, NOT an installer: " + target["id"]).encode()
            package.write_bytes(payload)
            receipt = dict(schema_version=1, target=target["id"], version=VERSION, filename=package.name,
                           source_commit=COMMIT, sha256=hashlib.sha256(payload).hexdigest(), size=len(payload),
                           metadata=dict(version=VERSION, architecture=target.get("package_arch", target["architecture"])),
                           checks=sorted(release.required_checks(target, signed)), native=True,
                           signed=target["os"] == "macos" and signed)
            (directory / (package.name + ".json")).write_text(json.dumps(receipt), encoding="utf-8")
        return directory

    def collect(self, directory, signed=False):
        return release.collect_artifacts(directory, self.config, VERSION, COMMIT, signed)

    def publish_files(self):
        package = self.root / "unit-fixture.pkg"
        package.write_bytes(b"HTTP contract fixture, not an installer")
        checksums = self.root / "SHA256SUMS"
        checksums.write_text(hashlib.sha256(package.read_bytes()).hexdigest() + "  " + package.name + "\n")
        return [package, checksums]

    def test_stable_tags_and_msi_limits(self):
        for version in ("0.0.0", VERSION, "255.255.65535"):
            self.assertEqual(version, release.validate_tag("v" + version, version))
        for tag in ("1.2.3", "V1.2.3", "v01.2.3", "v1.02.3", "v1.2.03", "v1.2", "v-1.2.3",
                    "v1.2.3-rc.1", "v1.2.3+build", "v1.2.3\n", "v256.0.0", "v0.256.0", "v0.0.65536"):
            with self.subTest(tag=tag), self.assertRaises(release.ReleaseError):
                release.validate_tag(tag, tag.removeprefix("v"))
        with self.assertRaises(release.ReleaseError):
            release.validate_tag(TAG, "1.2.4")

    def test_matrix_names_checks_and_downloads(self):
        self.config = release.load_config()
        self.targets = release.targets(self.config)
        linux = {target["id"] for target in self.targets if target["os"] == "linux"}
        systems = ("debian-12", "debian-13", "ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04",
                   "fedora-43", "fedora-44", "rhel-8.10", "rhel-9.8", "rhel-10.2")
        self.assertEqual({f"{system}-{arch}" for system in systems for arch in ("x86_64", "arm64")}, linux)
        self.assertEqual(22, len({target["id"] for target in self.targets}))
        self.assertEqual({"linux": 20, "macos": 1, "windows": 1},
                         {os: sum(t["os"] == os for t in self.targets) for os in ("linux", "macos", "windows")})
        base = {"build", "tests", "metadata", "install", "uninstall", "catalog", "data", "native"}
        extra = {"linux": {"desktop", "udev"}, "macos": {"bundle"},
                 "windows": {"shortcut", "static_runtime"}}
        for signed in (False, True):
            table = release.download_table(self.config, VERSION, "owner/repo", signed)
            for target in self.targets:
                suffix = ("-signed" if signed else "-unsigned") if target["os"] == "macos" else ""
                name = f"glasses-broadcaster-{VERSION}-{target['id']}{suffix}.{target['format']}"
                self.assertEqual(name, release.artifact_name(target, VERSION, signed))
                self.assertIn(f"https://github.com/owner/repo/releases/download/{TAG}/{name}", table)
                signing = {"signature", "timestamp", "notarization"} if signed and target["os"] == "macos" else set()
                self.assertEqual(base | extra[target["os"]] | signing, release.required_checks(target, signed))
            self.assertIn("signed/notarized" if signed else "unsigned", table)
        unsigned_table = release.download_table(self.config, VERSION, "owner/repo")
        self.assertIn("open the unsigned `.msi`", unsigned_table)
        self.assertIn("Unknown Publisher or SmartScreen warnings", unsigned_table)

    def test_invalid_matrices_fail_closed(self):
        for defect in ("schema", "missing-target", "missing-architecture", "duplicate", "unpinned"):
            config = deepcopy(release.load_config())
            if defect == "schema":
                config["schema_version"] = 2
            elif defect == "missing-target":
                config["linux"].pop()
            elif defect == "missing-architecture":
                config["linux"][0]["images"].pop("arm64")
            elif defect == "duplicate":
                config["linux"].append(deepcopy(config["linux"][0]))
            else:
                config["linux"][0]["images"]["arm64"] = "latest"
            path = self.root / "targets.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            with self.subTest(defect=defect), self.assertRaises(release.ReleaseError):
                release.load_config(path)

    def test_complete_contract_bundles_and_sha256(self):
        path = self.root / "abc"
        path.write_bytes(b"abc")
        self.assertEqual("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", release.sha256(path))
        for signed in (False, True):
            self.assertEqual(22, len(self.collect(self.bundle(signed), signed)))

    def test_receipt_fields_and_each_required_check(self):
        for signed in (False, True):
            directory = self.bundle(signed)
            for os in ("linux", "windows", "macos"):
                target = next(t for t in self.targets if t["os"] == os)
                path = directory / (release.artifact_name(target, VERSION, signed) + ".json")
                original = json.loads(path.read_text())
                changes = [{key: None} for key in original]
                changes += [{"schema_version": 2}, {"target": "wrong"}, {"version": "9.9.9"}, {"filename": "wrong"}, {"native": False},
                            {"signed": not original["signed"]}, {"source_commit": "b" * 40}, {"size": original["size"] + 1},
                            {"sha256": "0" * 64}, {"metadata": dict(version="9.9.9", architecture=original["metadata"]["architecture"])},
                            {"metadata": dict(version=VERSION, architecture="wrong")}]
                changes += [{"checks": [c for c in original["checks"] if c != check]} for check in original["checks"]]
                for change in changes:
                    path.write_text(json.dumps({key: value for key, value in (original | change).items() if value is not None}))
                    with self.subTest(os=os, signed=signed, change=change), self.assertRaises(release.ReleaseError):
                        self.collect(directory, signed)
                path.write_text(json.dumps(original))

    def test_receipts_require_a_full_hex_commit(self):
        directory = self.bundle()
        for path in directory.glob("*.json"):
            receipt = json.loads(path.read_text())
            receipt["source_commit"] = "z" * 40
            path.write_text(json.dumps(receipt))
        with self.assertRaises(release.ReleaseError):
            release.collect_artifacts(directory, self.config, VERSION, "z" * 40)

    def test_windows_attestation_accepts_only_unsigned_installer_proofs(self):
        config = release.load_config()
        target = config["windows"]
        package = self.root / release.artifact_name(target, VERSION)
        package.write_bytes(b"Unit fixture, NOT a Windows installer")
        destination = self.root / "attested"
        proof_path = self.root / "windows-verification.json"
        proof = dict(version=VERSION, architecture="x86_64", installed=True, uninstalled=True,
                     signed=False, sha256=release.sha256(package))

        def attest_with(current_proof):
            proof_path.write_text(json.dumps(current_proof), encoding="utf-8")
            with patch.object(release, "native_architecture", return_value="x86_64"), \
                    patch.object(release.platform, "system", return_value="Windows"):
                release.attest(config, target, VERSION, package, destination, proof_path)

        attest_with(proof)
        receipt_path = destination / (package.name + ".json")
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertFalse(receipt["signed"])
        self.assertNotIn("signature", receipt["checks"])
        self.assertNotIn("timestamp", receipt["checks"])

        for defect in (
            {"installed": False},
            {"uninstalled": False},
            {"sha256": "0" * 64},
            {"signed": True},
        ):
            with self.subTest(defect=defect), self.assertRaises(release.ReleaseError):
                attest_with(proof | defect)

        macos = next(target for target in release.targets(config) if target["os"] == "macos")
        self.assertTrue({"signature", "timestamp", "notarization"}.issubset(
            release.required_checks(macos, macos_sign=True)))

        mac_package = self.root / release.artifact_name(macos, VERSION, macos_sign=True)
        mac_package.write_bytes(b"Unit fixture, NOT a macOS installer")
        mac_destination = self.root / "attested-macos"
        mac_proof_path = self.root / "macos-verification.json"
        mac_proof = dict(version=VERSION, architecture="arm64", installed=True, uninstalled=True,
                         signed=True, timestamped=True, notarized=True, sha256=release.sha256(mac_package))
        mac_metadata = dict(version=VERSION, architecture="arm64", minimum_os="11.0")

        def attest_macos(current_proof):
            mac_proof_path.write_text(json.dumps(current_proof), encoding="utf-8")
            with patch.object(release, "native_architecture", return_value="arm64"), \
                    patch.object(release.platform, "system", return_value="Darwin"), \
                    patch.object(release, "macos_metadata", return_value=mac_metadata):
                release.attest(config, macos, VERSION, mac_package, mac_destination, mac_proof_path, macos_sign=True)

        for defect in ({"timestamped": False}, {"notarized": False}):
            with self.subTest(macos_defect=defect), self.assertRaises(release.ReleaseError):
                attest_macos(mac_proof | defect)
        attest_macos(mac_proof)
        mac_receipt = json.loads((mac_destination / (mac_package.name + ".json")).read_text(encoding="utf-8"))
        self.assertTrue(mac_receipt["signed"])
        self.assertTrue({"signature", "timestamp", "notarization"}.issubset(mac_receipt["checks"]))

    def test_bundle_completeness_duplicates_symlinks_and_corruption(self):
        for defect in ("package", "receipt", "extra", "duplicate", "symlink", "corrupt"):
            directory = self.bundle()
            package = next(directory.glob("*.deb"))
            if defect in ("package", "receipt"):
                (package if defect == "package" else Path(str(package) + ".json")).unlink()
            elif defect == "extra":
                (directory / "unexpected").write_bytes(b"extra")
            elif defect == "duplicate":
                (directory / "nested").mkdir()
                (directory / "nested" / package.name).write_bytes(package.read_bytes())
            elif defect == "symlink":
                outside = self.root / "linked-fixture"
                outside.write_bytes(package.read_bytes())
                package.unlink()
                package.symlink_to(outside)
            else:
                package.write_bytes(b"x" * package.stat().st_size)
            with self.subTest(defect=defect), self.assertRaises(release.ReleaseError):
                self.collect(directory)

    def test_incomplete_draft_retry_and_stale_asset_repair(self):
        files = self.publish_files()
        for stale in (False, True):
            api = HTTPFixture("upload")
            with self.assertRaises(release.ReleaseError):
                release.publish(api, TAG, COMMIT, files, "Downloads")
            self.assertFalse(any(method == "PATCH" for method, *_ in api.calls))
            self.assertTrue(api.release["draft"])
            if stale:
                api.assets[0]["digest"] = "sha256:" + "0" * 64
            api.fault = None
            release.publish(api, TAG, COMMIT, files, "Downloads")
            self.assertFalse(api.release["draft"])
            self.assertIn("Downloads\n### Release notes\n\nGenerated notes", api.release["body"])
            self.assertEqual(1, sum(method == "POST" and path == "/releases" for method, path, *_ in api.calls))
            self.assertEqual(int(stale), sum(method == "DELETE" for method, *_ in api.calls))
            self.assertEqual({path.name for path in files}, {asset["name"] for asset in api.assets})

    def test_published_or_foreign_drafts_are_never_mutated(self):
        for draft, commit in ((False, COMMIT), (True, "b" * 40)):
            api = HTTPFixture()
            api.release = dict(draft=draft, target_commitish=commit)
            with self.assertRaises(release.ReleaseError):
                release.publish(api, TAG, COMMIT, self.publish_files(), "Downloads")
            self.assertTrue(all(method == "GET" for method, *_ in api.calls))

    def test_unsafe_uploads_cannot_publish(self):
        for fault in ("missing", "duplicate", "unexpected", "size", "corrupt-unit-fixture.pkg", "corrupt-SHA256SUMS", "tag", "upload"):
            api = HTTPFixture(fault)
            with self.subTest(fault=fault), self.assertRaises(release.ReleaseError):
                release.publish(api, TAG, COMMIT, self.publish_files(), "Downloads")
            self.assertFalse(any(method == "PATCH" and data.get("draft") is False for method, _, data, _ in api.calls))
            self.assertTrue(api.release["draft"])

    def test_prior_msi_selection_and_download_integrity(self):
        # These bytes exercise HTTP selection/integrity, never MSI installation.
        self.config = release.load_config()
        payload = b"unit-only prior download fixture"
        source = self.root / "prior-download-bytes"
        source.write_bytes(payload)
        digest = "sha256:" + release.sha256(source)

        def prior(tag, identifier, draft=False, prerelease=False):
            return dict(tag_name=tag, draft=draft, prerelease=prerelease, assets=[
                dict(id=identifier, name=release.artifact_name(self.config["windows"], tag[1:]), digest=digest)
            ])

        for scenario in ("latest", "none", "corrupt", "duplicate"):
            entries = [prior("v0.1.9", 9), prior("v0.2.0", 20), prior("v0.1.10", 10),
                       prior("v0.1.12", 12, draft=True), prior("v0.1.11-beta.1", 11, prerelease=True)]
            if scenario == "none":
                entries = entries[1:2]
            elif scenario == "corrupt":
                entries[2]["assets"][0]["digest"] = "sha256:" + "0" * 64
            elif scenario == "duplicate":
                entries[2]["assets"].append(dict(entries[2]["assets"][0]))

            class ReadOnlyHTTP:
                def call(self, method, path, data=None, *, binary=False):
                    if method != "GET":
                        raise AssertionError("Prior-release selection must be read-only")
                    if path == "/releases?per_page=100&page=1":
                        return entries
                    if path == "/releases/assets/10" and binary:
                        return payload
                    raise AssertionError((method, path))

            destination = self.root / ("prior-" + scenario)
            with self.subTest(scenario=scenario):
                if scenario in ("corrupt", "duplicate"):
                    with self.assertRaises(release.ReleaseError):
                        release.previous_msi(ReadOnlyHTTP(), self.config, "0.2.0", destination)
                    self.assertFalse(destination.exists())
                else:
                    path = release.previous_msi(ReadOnlyHTTP(), self.config, "0.2.0", destination)
                    if scenario == "none":
                        self.assertIsNone(path)
                    else:
                        self.assertEqual(release.artifact_name(self.config["windows"], "0.1.10"), path.name)
                        self.assertEqual(payload, path.read_bytes())


if __name__ == "__main__":
    unittest.main()