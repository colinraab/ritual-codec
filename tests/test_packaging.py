#!/usr/bin/env python3
"""Release archive regression checks using isolated fake build trees."""
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import subprocess
import zipfile

spec = importlib.util.spec_from_file_location('packager', Path(__file__).resolve().parents[1] / 'tools/package_release.py')
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.base = Path(tempfile.mkdtemp(prefix="ritual-packaging-test-"))
        self.addCleanup(pkg.discard_generated, self.base)
        self.root = self.base / 'project'
        self.build = self.root / 'build'
        self.output = self.root / 'dist'
        self.build.mkdir(parents=True)
        self.output.mkdir()
        for name in ['LICENSE', 'LICENSE-NOTES.md', 'ATTRIBUTION.md', 'README.md',
                     'CMakeLists.txt', 'reports/source_provenance.json', 'docs/BUILDING.md',
                     'docs/INSTALLING.md', 'docs/PACKAGING.md',
                     'assets/fonts/OFL.txt', 'assets/fonts/CormorantGaramond-OFL.txt',
                     'src/main.cpp', 'src/rit_main.cpp']:
            self.write(self.root / name, 'test fixture\n')
        for name in ['ritual-codec', 'ritual-stream']:
            self.write(self.build / name, 'fake executable\n')
            (self.build / name).chmod(0o755)

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data)

    def cli_archive(self):
        return pkg.make_archive(self.root, self.build, self.output, 'test', 'cli',
                                pkg.find_cli_outputs(self.build))

    def test_both_clis_and_curated_source(self):
        self.write(self.root / 'private.txt', 'DO NOT PACKAGE')
        self.write(self.root / 'src/.env', 'DO NOT PACKAGE')
        self.write(self.root / 'reports/local.log', 'DO NOT PACKAGE')
        archive = self.cli_archive()
        with zipfile.ZipFile(archive) as z:
            self.assertIn('bin/ritual-codec', z.namelist())
            self.assertIn('bin/ritual-stream', z.namelist())
            self.assertIn('SHA256SUMS.txt', z.namelist())
            if os.name != 'nt':
                self.assertEqual((z.getinfo('bin/ritual-stream').external_attr >> 16) & 0o777, 0o755)
            with zipfile.ZipFile(io.BytesIO(z.read('source/ritual-codec-source.zip'))) as src:
                self.assertIn('ritual-codec/src/rit_main.cpp', src.namelist())
                self.assertNotIn('ritual-codec/private.txt', src.namelist())
                self.assertNotIn('ritual-codec/src/.env', src.namelist())
                self.assertFalse(any('/build/' in n or '/dist/' in n for n in src.namelist()))

    def test_missing_stream_is_error(self):
        (self.build / 'ritual-stream').rename(self.base / 'preserved-stream')
        with self.assertRaises(pkg.PackagingError):
            pkg.find_cli_outputs(self.build)
        self.assertEqual(list(self.output.glob('*.zip')), [])

    def test_duplicate_configurations_are_error(self):
        self.write(self.build / 'Debug/ritual-codec', 'duplicate')
        with self.assertRaises(pkg.PackagingError):
            pkg.find_cli_outputs(self.build)

    def dependency_fixture(self):
        juce = self.base / 'local-JUCE'
        for name in ['LICENSE.md', 'JUCE.spdx.json', 'CMakeLists.txt',
                     'modules/thirdparty/NOTICE', 'extras/Build/CMake/support.cmake']:
            self.write(juce / name, 'upstream fixture\n')
        self.write(juce / '.git/config', 'DO NOT PACKAGE')
        self.write(juce / 'build/cache', 'DO NOT PACKAGE')
        self.write(juce / 'modules/juce_core/system/juce_StandardHeader.h',
                   '#define JUCE_MAJOR_VERSION 9\n#define JUCE_MINOR_VERSION 0\n#define JUCE_BUILDNUMBER 2\n')
        self.write(self.build / 'CMakeCache.txt', f'RITUAL_JUCE_SOURCE_DIR:PATH={juce}\n')
        return juce

    def test_plugin_bundles_configured_dependency_and_notices(self):
        self.dependency_fixture()
        bundle = self.build / 'Release/Ritual.vst3'
        self.write(bundle / 'Contents/test.bin', 'plugin fixture')
        real_run = subprocess.run
        def fake_signature_only(command, **kwargs):
            if command[0] == '/usr/bin/codesign':
                return subprocess.CompletedProcess(command, 0)
            return real_run(command, **kwargs)
        # Fixture bundles are not real Mach-O binaries. Preserve native ditto
        # verification, mocking only the signature check for this fixture.
        with mock.patch.object(pkg.subprocess, 'run', side_effect=fake_signature_only):
            archive = pkg.make_archive(self.root, self.build, self.output, 'test', 'vst3', [bundle])
        with zipfile.ZipFile(archive) as z:
            provenance = z.read('BUILD_PROVENANCE.txt').decode()
            self.assertIn('local source override', provenance)
            self.assertNotIn(str(self.base), provenance)
            with zipfile.ZipFile(io.BytesIO(z.read('source/ritual-codec-source.zip'))) as src:
                names = src.namelist()
                self.assertIn('ritual-codec/JUCE/modules/thirdparty/NOTICE', names)
                self.assertIn('ritual-codec/JUCE/extras/Build/CMake/support.cmake', names)
                self.assertFalse(any('/.git/' in n or '/build/' in n for n in names))

    def test_standalone_archive_contains_app_icon_and_corresponding_source(self):
        self.dependency_fixture()
        bundle = self.build / 'RitualCodecApp_artefacts/Release/Ritual Codec.app'
        self.write(bundle / 'Contents/MacOS/Ritual Codec', 'app fixture')
        (bundle / 'Contents/MacOS/Ritual Codec').chmod(0o755)
        self.write(bundle / 'Contents/Resources/AppIcon.icns', 'icon fixture')
        self.write(self.root / 'standalone/Main.cpp', 'standalone source')
        self.write(self.root / 'standalone/assets/generate-icon.swift', 'icon source')
        self.write(self.build / '_deps/JUCE/helper.app/Contents/helper', 'exclude helper')
        self.assertEqual(pkg.find_standalone_outputs(self.build), [bundle])
        real_run = subprocess.run
        def fake_signature_only(command, **kwargs):
            if command[0] == '/usr/bin/codesign':
                return subprocess.CompletedProcess(command, 0)
            return real_run(command, **kwargs)
        with mock.patch.object(pkg.subprocess, 'run', side_effect=fake_signature_only) as run:
            archive = pkg.make_archive(self.root, self.build, self.output, 'macos-arm64',
                                       'standalone', [bundle], '0.2.0-preview.1')
        self.assertEqual(archive.name, 'ritual-codec-0.2.0-preview.1-macos-arm64-standalone.zip')
        if pkg.sys.platform == 'darwin':
            self.assertTrue(any(call.args[0][0] == '/usr/bin/codesign'
                                and call.args[0][-1].endswith('.app') for call in run.call_args_list))
        with zipfile.ZipFile(archive) as z:
            self.assertIn('STANDALONE/Ritual Codec.app/Contents/Resources/AppIcon.icns', z.namelist())
            with zipfile.ZipFile(io.BytesIO(z.read('source/ritual-codec-source.zip'))) as src:
                self.assertIn('ritual-codec/standalone/Main.cpp', src.namelist())
                self.assertIn('ritual-codec/standalone/assets/generate-icon.swift', src.namelist())
                self.assertIn('ritual-codec/JUCE/CMakeLists.txt', src.namelist())
        duplicate = self.build / 'RitualCodecApp_artefacts/Debug/Ritual Codec.app'
        duplicate.mkdir(parents=True)
        with self.assertRaises(pkg.PackagingError):
            pkg._validate_sources('standalone', pkg.find_standalone_outputs(self.build), self.build)

    def test_conflicting_dependency_paths_are_error(self):
        self.write(self.build / 'CMakeCache.txt',
                   'RITUAL_JUCE_SOURCE_DIR:PATH=/first\njuce_SOURCE_DIR:STATIC=/second\n')
        with self.assertRaises(pkg.PackagingError):
            pkg._resolve_juce_source(self.root, self.build)

    def test_archive_changed_bytes_detected(self):
        stage = self.base / 'stage'
        self.write(stage / 'file.txt', 'original')
        archive = self.base / 'test.zip'
        pkg.zip_stage(stage, archive)
        (stage / 'file.txt').write_text('changed')
        with self.assertRaises(pkg.PackagingError):
            pkg.verify_archive(stage, archive)

    def test_unsafe_archive_names_rejected(self):
        for name in ['', '.', '../outside', '/absolute', 'C:/outside', r'..\outside']:
            with self.subTest(name=name), self.assertRaises(pkg.PackagingError):
                pkg._check_archive_path(name)


if __name__ == '__main__':
    unittest.main()
