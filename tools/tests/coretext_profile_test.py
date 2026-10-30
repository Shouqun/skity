# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Permanent fail-closed controls for explicit native text references."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

import coretext_profile as profile


class ProfileContract(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sources = {}
        for relative in profile.REQUIRED_SOURCES:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(relative)
            self.sources[relative] = profile.identity(path)["sha256"]
        self.font = self.root / "font.ttc"
        self.font.write_bytes(b"native font identity fixture")
        self.declaration = {
            "sources": self.sources,
            "platform": {"system": "Darwin", "version": "fixture"},
            "native_recipe": {"width": 400, "height": 400, "tracking": 0},
            "fonts": {"latin": {"postscript": "Fixture", "version": "1",
                                "head_hex": "abcd",
                                "file_identity": profile.identity(self.font)}},
        }
        self.metadata = {
            "schema_version": 1,
            "producer_source_sha256": self.sources[
                "test/golden/native_reference/coretext_reference.cc"],
            "platform": copy.deepcopy(self.declaration["platform"]),
            "width": 400, "height": 400, "tracking": 0,
            "fonts": {"latin": {"postscript": "Fixture", "version": "1",
                                "head_hex": "abcd", "file": str(self.font)}},
        }

    def prepared(self):
        images = {}
        for name in profile.CASES.values():
            (self.root / name).write_bytes(b"reference " + name.encode())
            images[name] = profile.identity(self.root / name)
        inputs = profile.source_identities(self.declaration, self.root)
        inputs[str(self.font)] = profile.identity(self.font)
        return profile.PreparedProfile("fixture", self.root, self.declaration,
                                       self.metadata, inputs, images)

    def report(self):
        cases = []
        for full, filename in profile.CASES.items():
            cases.append({"name": full.split(".")[1], "result": "COMPLETED",
                          "coretext_profile": "fixture",
                          "coretext_profile_reference": str(self.root / filename),
                          "coretext_profile_recipe_sha256": self.sources[
                              "test/golden/cases/text/text.cc"],
                          "coretext_profile_checker_sha256": self.sources[
                              "test/golden/common/golden_test_check.cc"]})
        return [{"testsuites": [{"name": "TextGolden", "testsuite": cases}]}]

    def test_unknown_profile_refuses_without_execution(self):
        with patch.object(profile.subprocess, "run") as run:
            with self.assertRaisesRegex(profile.ProfileError, "Unknown CoreText"):
                profile.prepare_profile(self.root, self.root, "unknown", "metal")
            run.assert_not_called()

    def test_native_producer_requires_only_declared_system_dependencies(self):
        output = "producer:\n\t/System/Library/Frameworks/CoreText.framework/Versions/A/CoreText (compatibility version 1.0.0)\n\t/usr/lib/libc++.1.dylib (compatibility version 1.0.0)\n"
        with patch.object(profile.subprocess, "check_output", return_value=output):
            self.assertEqual(profile.native_dependencies("producer"), output)
        for additional in ("\t/tmp/libskity.dylib (compatibility version 1.0.0)\n", "\t/tmp/libother.dylib (compatibility version 1.0.0)\n"):
            with patch.object(profile.subprocess, "check_output", return_value=output + additional):
                with self.assertRaisesRegex(profile.ProfileError, "non-native dependency"):
                    profile.native_dependencies("producer")

    def test_sanitizer_runtime_requires_matching_compiler_and_loader(self):
        compiler = self.root / "compiler"
        compiler.write_bytes(b"configured compiler fixture")
        runtime = self.root / "toolchain" / "libclang_rt.asan_osx_dynamic.dylib"
        runtime.parent.mkdir()
        runtime.write_bytes(b"configured compiler runtime fixture")
        cache = self.root / "CMakeCache.txt"
        cache.write_text("CMAKE_CXX_COMPILER:FILEPATH=" + str(compiler) +
                         "\nCMAKE_CXX_FLAGS:STRING=-fsanitize=address,undefined\n")
        dependency = "producer:\n\t@rpath/libclang_rt.asan_osx_dynamic.dylib (compatibility version 0.0.0)\n"
        loader = "cmd LC_RPATH\n cmdsize 32\n path @executable_path (offset 12)\ncmd LC_RPATH\n cmdsize 120\n path " + str(runtime.parent) + " (offset 12)\n"
        def output(command, **kwargs):
            if command[:2] == ["/usr/bin/otool", "-L"]:
                return dependency
            if command[:2] == ["/usr/bin/otool", "-l"]:
                return loader
            self.assertEqual(command, [str(compiler), "-print-file-name=libclang_rt.asan_osx_dynamic.dylib"])
            return str(runtime) + "\n"
        with patch.object(profile.subprocess, "check_output", side_effect=output):
            inputs, evidence = {}, {}
            profile.native_dependencies(self.root / "producer", self.root, inputs, evidence)
            self.assertEqual(inputs[str(runtime.resolve())], profile.identity(runtime))
            self.assertEqual(inputs[str(compiler)], profile.identity(compiler))
            self.assertEqual(inputs[str(cache)], profile.identity(cache))
            self.assertEqual(evidence["runtime"], str(runtime.resolve()))
            (self.root / runtime.name).write_bytes(b"shadow runtime")
            with self.assertRaisesRegex(profile.ProfileError, "shadowed"):
                profile.native_dependencies(self.root / "producer", self.root)
            (self.root / runtime.name).unlink()
            cache.write_text("CMAKE_CXX_COMPILER:FILEPATH=" + str(compiler) +
                             "\nCMAKE_CXX_FLAGS:STRING=-O2\n")
            with self.assertRaisesRegex(profile.ProfileError, "instrumentation"):
                profile.native_dependencies(self.root / "producer", self.root)

    def test_sanitizer_dependency_requires_configured_evidence(self):
        output = "producer:\n\t@rpath/libclang_rt.asan_osx_dynamic.dylib (compatibility version 0.0.0)\n"
        with patch.object(profile.subprocess, "check_output", return_value=output):
            with self.assertRaisesRegex(profile.ProfileError, "configured compiler evidence"):
                profile.native_dependencies("producer")

    def test_native_environment_clears_loader_overrides(self):
        keys = (profile.PROFILE_ENV, profile.DIRECTORY_ENV, "DYLD_LIBRARY_PATH",
                "DYLD_FRAMEWORK_PATH", "DYLD_INSERT_LIBRARIES")
        with patch.dict(os.environ, dict.fromkeys(keys, "/foreign")):
            environment = profile.native_environment()
        self.assertTrue(all(key not in environment for key in keys))

    def test_profile_path_traversal_refuses(self):
        with self.assertRaisesRegex(profile.ProfileError, "Invalid CoreText"):
            profile.prepare_profile(self.root, self.root, "../../foreign", "metal")

    def test_legacy_does_not_read_files_or_execute(self):
        with patch.object(profile.subprocess, "run") as run:
            self.assertIsNone(profile.prepare_profile("/missing", "/missing", "legacy", "gl"))
            run.assert_not_called()

    def test_complete_source_inventory_is_required(self):
        del self.declaration["sources"]["tools/test-runner.py"]
        with self.assertRaisesRegex(profile.ProfileError, "source pins"):
            profile.source_identities(self.declaration, self.root)

    def test_changed_source_refuses(self):
        (self.root / "tools/test-runner.py").write_text("changed")
        with self.assertRaisesRegex(profile.ProfileError, "changed"):
            profile.source_identities(self.declaration, self.root)

    def test_native_identity_positive_and_unknown_fields_refuse(self):
        self.assertEqual(profile.validate_native(self.declaration, self.metadata),
                         {str(self.font): profile.identity(self.font)})
        for key, value in (("platform", {}), ("width", 401), ("tracking", 1),
                           ("producer_source_sha256", "stale"), ("fonts", {}),
                           ("schema_version", 2)):
            with self.subTest(field=key):
                changed = copy.deepcopy(self.metadata)
                changed[key] = value
                with self.assertRaises(profile.ProfileError):
                    profile.validate_native(self.declaration, changed)

    def test_changed_font_name_version_table_or_file_refuses(self):
        for key, value in (("postscript", "substitute"), ("version", "2"),
                           ("head_hex", "ffff"), ("file", "relative.ttc")):
            with self.subTest(field=key):
                changed = copy.deepcopy(self.metadata)
                changed["fonts"]["latin"][key] = value
                with self.assertRaises(profile.ProfileError):
                    profile.validate_native(self.declaration, changed)
        self.font.write_bytes(b"changed bytes")
        with self.assertRaisesRegex(profile.ProfileError, "fingerprint changed"):
            profile.validate_native(self.declaration, self.metadata)

    def test_both_executed_acknowledged_cases_are_required(self):
        prepared = self.prepared()
        prepared.verify(self.report())
        self.assertTrue(prepared.verified)
        for mutation in ("missing", "duplicate", "skip", "failure", "ack", "path", "stale_recipe"):
            with self.subTest(mutation=mutation):
                reports = self.report()
                cases = reports[0]["testsuites"][0]["testsuite"]
                if mutation == "missing":
                    cases.pop()
                elif mutation == "duplicate":
                    cases.append(copy.deepcopy(cases[0]))
                elif mutation == "skip":
                    cases[0]["result"] = "SKIPPED"
                elif mutation == "failure":
                    cases[0]["failures"] = [{"failure": "original pixel mismatch"}]
                elif mutation == "ack":
                    cases[0].pop("coretext_profile")
                elif mutation == "path":
                    cases[0]["coretext_profile_reference"] = "/foreign.png"
                else:
                    cases[0]["coretext_profile_recipe_sha256"] = "stale"
                prepared.verified = False
                with self.assertRaises(profile.ProfileError):
                    prepared.verify(reports)
                self.assertFalse(prepared.verified)

    def test_input_and_reference_changes_during_execution_refuse(self):
        prepared = self.prepared()
        self.font.write_bytes(b"changed during run")
        with self.assertRaisesRegex(profile.ProfileError, "changed during test"):
            prepared.verify(self.report())
        self.font.write_bytes(b"native font identity fixture")
        (self.root / next(iter(profile.CASES.values()))).write_bytes(b"changed image")
        with self.assertRaisesRegex(profile.ProfileError, "reference changed"):
            prepared.verify(self.report())

    def test_executed_image_is_bound_and_changes_refuse(self):
        prepared = self.prepared()
        executable = self.root / "golden-test-image"
        executable.write_bytes(b"executed image fixture")
        prepared.bind_test_images([executable])
        prepared.verify(self.report())
        self.assertEqual(prepared.report()["test_images"][str(executable.resolve())],
                         profile.identity(executable))
        self.assertTrue((self.root / "executed-tests.json").is_file())
        executable.write_bytes(b"changed executed image")
        with self.assertRaisesRegex(profile.ProfileError, "changed during test"):
            prepared.verify(self.report())
        self.assertFalse(prepared.verified)

    def test_native_extent_and_clear_background(self):
        with self.assertRaises(profile.ProfileError):
            profile.rgba_references(b"\0")
        images = profile.rgba_references(bytes(400 * 400))
        self.assertEqual(images["text_basic.png"], bytes(400 * 400 * 4))
        self.assertEqual(images["text_linear_gradient_flags.png"], b"\xff" * (400 * 400 * 4))

    def test_alpha_and_distinct_gradient_interpolation(self):
        coverage = bytearray(400 * 400)
        for y in (100, 300):
            coverage[y * 400 + 17] = 128
        images = profile.rgba_references(coverage)
        basic = images["text_basic.png"]
        index = (100 * 400 + 17) * 4
        self.assertEqual(basic[index:index + 4], bytes((255, 0, 0, 128)))
        second = (300 * 400 + 17) * 4
        self.assertEqual(basic[second:second + 4], bytes(4))
        gradient = images["text_linear_gradient_flags.png"]
        alpha, opacity = 0.5, 128 / 255
        for channel, color in enumerate((0.9019, 0.3921, 0.3960)):
            self.assertEqual(gradient[index + channel], round(255 * (1 - alpha * opacity + color * alpha * alpha * opacity)))
            self.assertEqual(gradient[second + channel], round(255 * (1 - alpha * opacity + color * alpha * opacity)))
        self.assertNotEqual(gradient[index:index + 3], gradient[second:second + 3])

    def test_corrupted_references_are_independent_and_specific(self):
        coverage = bytearray(400 * 400)
        for y in (100, 300):
            coverage[y * 400 + 17] = 128
        original = profile.rgba_references(coverage)
        shifted = profile.negative_references(coverage, "glyph-displacement")
        baseline = profile.negative_references(coverage, "baseline-displacement")
        alpha = profile.negative_references(coverage, "coverage-alpha")
        interpolation = profile.negative_references(coverage, "gradient-interpolation")
        basic = "text_basic.png"
        self.assertEqual(shifted[basic][(100 * 400 + 19) * 4 + 3], 128)
        self.assertEqual(shifted[basic][(100 * 400 + 17) * 4 + 3], 0)
        self.assertEqual(baseline[basic][(102 * 400 + 17) * 4 + 3], 128)
        self.assertEqual(alpha[basic][(100 * 400 + 17) * 4 + 3], 96)
        self.assertEqual(interpolation[basic], original[basic])
        self.assertNotEqual(interpolation["text_linear_gradient_flags.png"],
                            original["text_linear_gradient_flags.png"])
        for mutated in (shifted, baseline, alpha):
            for name in original:
                self.assertNotEqual(mutated[name], original[name])
                self.assertEqual(len(mutated[name]), 400 * 400 * 4)

    def test_png_crc_and_exact_pixels(self):
        pixels = bytes(range(256)) * 2500
        encoded = profile.png_bytes(pixels)
        self.assertEqual(encoded[:8], b"\x89PNG\r\n\x1a\n")
        offset, chunks = 8, {}
        while offset < len(encoded):
            length = struct.unpack(">I", encoded[offset:offset + 4])[0]
            kind = encoded[offset + 4:offset + 8]
            body = encoded[offset + 8:offset + 8 + length]
            checksum = struct.unpack(">I", encoded[offset + 8 + length:offset + 12 + length])[0]
            self.assertEqual(checksum, zlib.crc32(kind + body) & 0xffffffff)
            chunks[kind] = body
            offset += length + 12
        self.assertEqual(struct.unpack(">II", chunks[b"IHDR"][:8]), (400, 400))
        raw = zlib.decompress(chunks[b"IDAT"])
        self.assertEqual(b"".join(raw[y * 1601 + 1:(y + 1) * 1601] for y in range(400)), pixels)

    def test_runner_clears_ambient_profile_selection(self):
        runner_path = Path(profile.__file__).with_name("test-runner.py")
        spec = importlib.util.spec_from_file_location("profile_test_runner", runner_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        runner = object.__new__(module.TestRunner)
        runner.golden_failures_dir = str(self.root)
        runner.suite = "coretext-profile-contract"
        with patch.dict(os.environ, {profile.PROFILE_ENV: "foreign", profile.DIRECTORY_ENV: "/foreign"}):
            environment, error = runner.prepare_test_environment()
        self.assertIsNone(error)
        self.assertNotIn(profile.PROFILE_ENV, environment)
        self.assertNotIn(profile.DIRECTORY_ENV, environment)
