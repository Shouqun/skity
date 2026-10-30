# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Explicit native-only CoreText references; legacy mode never redirects."""
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zlib

PROFILE_ENV = "SKITY_TEST_CORETEXT_PROFILE"
DIRECTORY_ENV = "SKITY_TEST_CORETEXT_REFERENCE_DIR"
CASES = {
    "TextGolden.Basic": "text_basic.png",
    "TextGolden.TextLinearGradientFlags": "text_linear_gradient_flags.png",
}
WIDTH = HEIGHT = 400
REQUIRED_SOURCES = {
    "test/golden/native_reference/coretext_reference.cc",
    "test/golden/cases/text/text.cc",
    "test/golden/common/golden_test_check.cc",
    "test/golden/common/golden_test_check.hpp",
    "tools/coretext_profile.py",
    "tools/test-runner.py",
    "test/golden/CMakeLists.txt",
    "tools/tests/coretext_profile_test.py",
}


class ProfileError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise ProfileError(message)


def identity(path):
    path = Path(path)
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return {"bytes": path.stat().st_size, "sha256": digest.hexdigest()}


def native_dependencies(producer, build=None, inputs=None, evidence=None):
    output = subprocess.check_output(["/usr/bin/otool", "-L", str(producer)], text=True)
    libraries = [line.strip().split(" (compatibility version", 1)[0]
                 for line in output.splitlines()[1:] if line.strip()]
    require(libraries, "Native producer has no dependency inventory")
    for library in libraries:
        if library == "@rpath/libclang_rt.asan_osx_dynamic.dylib":
            require(build is not None, "Sanitizer dependency requires configured compiler evidence")
            cache_path = Path(build) / "CMakeCache.txt"
            cache = cache_path.read_text()
            compiler = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, re.M)
            flags = re.search(r"^CMAKE_CXX_FLAGS:[^=]+=(.*)$", cache, re.M)
            require(compiler and flags and
                    re.search(r"-fsanitize=[^ ]*\baddress\b", flags[1]),
                    "ASan producer lacks its configured instrumentation")
            compiler = Path(compiler[1])
            require(compiler.is_absolute() and compiler.is_file(),
                    "Missing configured sanitizer compiler")
            runtime = Path(subprocess.check_output([
                str(compiler), "-print-file-name=libclang_rt.asan_osx_dynamic.dylib"], text=True).strip())
            require(runtime.is_absolute() and runtime.is_file(),
                    "Missing configured compiler sanitizer runtime")
            load_commands = subprocess.check_output(["/usr/bin/otool", "-l", str(producer)], text=True)
            rpaths = re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", load_commands)
            resolved = None
            for rpath in rpaths:
                if rpath.startswith("@executable_path") or rpath.startswith("@loader_path"):
                    rpath = str(Path(producer).resolve().parent) + rpath[rpath.index("/"):] if "/" in rpath else str(Path(producer).resolve().parent)
                directory = Path(rpath)
                require(directory.is_absolute(), "Unknown sanitizer runtime search path")
                candidate = directory / runtime.name
                if candidate.is_file():
                    resolved = candidate.resolve()
                    break
            require(resolved == runtime.resolve(),
                    "Sanitizer runtime is shadowed or not the configured compiler runtime")
            if inputs is not None:
                inputs.update({str(path): identity(path) for path in (cache_path, compiler, resolved)})
            if evidence is not None:
                evidence.update({"instrumentation_flags": flags[1],
                                 "compiler": str(compiler), "runtime": str(resolved),
                                 "rpaths": rpaths, "load_commands": load_commands})
            continue
        framework = any(library.startswith("/System/Library/Frameworks/" + name + ".framework/")
                        for name in ("CoreText", "CoreFoundation", "CoreGraphics"))
        require(framework or library in ("/usr/lib/libc++.1.dylib", "/usr/lib/libSystem.B.dylib"),
                "Native producer has a non-native dependency: " + library)
    return output


def native_environment():
    environment = os.environ.copy()
    for key in (PROFILE_ENV, DIRECTORY_ENV, "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
        environment.pop(key, None)
    return environment


def source_identities(profile, root):
    root = Path(root).resolve()
    result = {}
    require(set(profile.get("sources", {})) == REQUIRED_SOURCES,
            "Profile has incomplete or unknown source pins")
    for relative, expected in profile["sources"].items():
        path = (root / relative).resolve()
        require(path.is_relative_to(root) and path.is_file(),
                "Missing or foreign profile source: " + relative)
        actual = identity(path)
        require(actual["sha256"] == expected,
                "Profile source/recipe changed: " + relative)
        result[str(path)] = actual
    return result


def validate_native(profile, metadata, fingerprint=identity):
    require(metadata.get("schema_version") == 1, "Unknown native schema")
    require(metadata.get("platform") == profile["platform"],
            "Unknown native platform fingerprint")
    for key, expected in profile["native_recipe"].items():
        require(metadata.get(key) == expected,
                "Native recipe mismatch: " + key)
    producer = "test/golden/native_reference/coretext_reference.cc"
    require(metadata.get("producer_source_sha256") == profile["sources"][producer],
            "Native producer is stale or foreign")
    require(set(metadata.get("fonts", {})) == set(profile["fonts"]),
            "Incomplete native font inventory")
    files = {}
    for role, expected in profile["fonts"].items():
        actual = metadata["fonts"][role]
        for key in ("postscript", "version", "head_hex"):
            require(actual.get(key) == expected[key],
                    "Unknown native font " + role + "/" + key)
        path = Path(actual.get("file", ""))
        require(path.is_absolute(), "Native font has no genuine file URL")
        file_identity = fingerprint(path)
        require(file_identity == expected["file_identity"],
                "Native font file fingerprint changed: " + role)
        files[str(path)] = file_identity
    return files


def rgba_references(coverage):
    """Native A8 plus original mirror-gradient math, never product pixels."""
    require(len(coverage) == WIDTH * HEIGHT, "Native coverage extent mismatch")
    basic = bytearray(WIDTH * HEIGHT * 4)
    gradient = bytearray(b"\xff") * (WIDTH * HEIGHT * 4)
    colors = (0.9019, 0.3921, 0.3960)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            sample = coverage[y * WIDTH + x]
            index = (y * WIDTH + x) * 4
            # Basic contains the first two commands only. The original second
            # gradient iteration begins at y=200 and retains its CJK typeface.
            if y < 200 and sample:
                basic[index:index + 4] = bytes((255, 0, 0, sample))
            opacity = sample / 255
            t = ((x + 0.5) / 20) % 2
            t = 2 - t if t > 1 else t
            fraction = min(1.0, max(0.0, (t - 0.75) / 0.25))
            alpha = 1 - fraction
            factor = alpha * alpha if y < 200 else alpha
            for channel, color in enumerate(colors):
                gradient[index + channel] = round(
                    255 * (1 - alpha * opacity + color * factor * opacity))
    return dict(zip(CASES.values(), (basic, gradient)))


def png_bytes(rgba):
    require(len(rgba) == WIDTH * HEIGHT * 4, "Wrong reference pixel extent")
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff))
    raw = b"".join(b"\0" + rgba[y * WIDTH * 4:(y + 1) * WIDTH * 4]
                   for y in range(HEIGHT))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)) +
            chunk(b"sRGB", b"\0") + chunk(b"IDAT", zlib.compress(raw)) +
            chunk(b"IEND", b""))


NEGATIVES = {
    "glyph-displacement": set(CASES),
    "baseline-displacement": set(CASES),
    "coverage-alpha": set(CASES),
    "gradient-interpolation": {"TextGolden.TextLinearGradientFlags"},
}


def negative_references(coverage, kind):
    require(kind in NEGATIVES, "Unknown negative reference control")
    require(len(coverage) == WIDTH * HEIGHT, "Native coverage extent mismatch")
    changed = bytearray(len(coverage))
    if kind == "glyph-displacement":
        for y in range(HEIGHT):
            changed[y * WIDTH + 2:(y + 1) * WIDTH] = coverage[y * WIDTH:(y + 1) * WIDTH - 2]
    elif kind == "baseline-displacement":
        changed[2 * WIDTH:] = coverage[:-2 * WIDTH]
    elif kind == "coverage-alpha":
        changed[:] = bytes(round(value * 0.75) for value in coverage)
    else:
        images = rgba_references(coverage)
        gradient = images["text_linear_gradient_flags.png"]
        for y in range(HEIGHT):
            for x in range(WIDTH):
                opacity = coverage[y * WIDTH + x] / 255
                t = ((x + 0.5) / 20) % 2
                t = 2 - t if t > 1 else t
                alpha = 1 - min(1.0, max(0.0, (t - 0.75) / 0.25))
                # Deliberately reverse the two original interpolation flags.
                factor = alpha if y < 200 else alpha * alpha
                for channel, color in enumerate((0.9019, 0.3921, 0.3960)):
                    gradient[(y * WIDTH + x) * 4 + channel] = round(
                        255 * (1 - alpha * opacity + color * factor * opacity))
        return images
    return rgba_references(changed)


class PreparedProfile:
    def __init__(self, name, directory, profile, metadata, inputs, images):
        self.name = name
        self.directory = Path(directory)
        self.profile = profile
        self.metadata = metadata
        self.inputs = inputs
        self.images = images
        self.test_images = {}
        self.verified = False

    def bind_test_images(self, executables):
        require(executables, "No test image to verify")
        self.test_images = {str(Path(path).resolve()): identity(path)
                            for path in executables}
        self.inputs.update(self.test_images)
        (self.directory / "executed-tests.json").write_text(
            json.dumps(self.test_images, indent=2) + "\n")

    def environment(self):
        return {PROFILE_ENV: self.name, DIRECTORY_ENV: str(self.directory)}

    def report(self):
        return {"id": self.name, "verified": self.verified,
                "directory": str(self.directory), "required_cases": list(CASES),
                "native": self.metadata, "images": self.images,
                "test_images": self.test_images,
                "legacy_result": "Not rerun or relabelled by this profile"}

    def verify(self, raw_reports):
        self.verified = False
        for path, expected in self.inputs.items():
            require(identity(path) == expected,
                    "Profile source/font/producer changed during test: " + path)
        for name, expected in self.images.items():
            require(identity(self.directory / name) == expected,
                    "Native reference changed during test")
        observed = {}
        for raw in raw_reports:
            for group in raw.get("testsuites", []):
                for case in group.get("testsuite", []):
                    full_name = group["name"] + "." + case["name"]
                    if full_name in CASES:
                        require(full_name not in observed,
                                "Duplicate covered profile case")
                        observed[full_name] = case
        require(set(observed) == set(CASES),
                "Both covered CoreText cases must actually execute")
        for name, case in observed.items():
            require(case.get("result") == "COMPLETED" and not case.get("failures"),
                    "Covered case failed or skipped: " + name)
            expected = self.directory / CASES[name]
            require(case.get("coretext_profile") == self.name,
                    "Test did not acknowledge selected native profile")
            require(case.get("coretext_profile_reference") == str(expected),
                    "Test used a different reference")
            for field, source in (
                ("coretext_profile_recipe_sha256", "test/golden/cases/text/text.cc"),
                ("coretext_profile_checker_sha256", "test/golden/common/golden_test_check.cc"),
            ):
                require(case.get(field) == self.profile["sources"][source],
                        "Test executable has stale recipe/checker: " + source)
        self.verified = True


def prepare_profile(root, build, name, backend):
    if name == "legacy":
        return None
    root, build = Path(root).resolve(), Path(build).resolve()
    require(re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,95}", name) is not None,
            "Invalid CoreText profile name")
    path = root / "test/golden/native_reference/profiles" / (name + ".json")
    require(path.is_file(), "Unknown CoreText profile: " + name)
    require(sys.platform == "darwin" and backend == "metal",
            "Native CoreText profile requires its declared macOS/Metal path")
    profile = json.loads(path.read_text())
    require(profile.get("schema_version") == 1 and profile.get("id") == name,
            "Invalid profile declaration")
    require(profile.get("cases") == CASES, "Profile changes covered workloads")
    require(profile.get("backend") == backend, "Profile backend mismatch")
    require(set(profile.get("fonts", {})) == {"latin", "cjk"},
            "Profile must inventory both original native faces")
    require(set(profile.get("platform", {})) == {
        "system", "machine", "product_version", "build_version", "coretext_bundle_version"},
        "Incomplete platform declaration")
    require(all(isinstance(value, str) and value for value in profile["platform"].values()),
            "Empty platform declaration")
    fixed_recipe = {"width": WIDTH, "height": HEIGHT, "format": "A8", "text_size": 64,
                    "optical_size": 12, "tracking": 0, "phase_steps": 4}
    recipe = profile.get("native_recipe", {})
    require(set(recipe) == set(fixed_recipe) | {"cjk_glyphs", "cjk_advances", "native_y0_at_row0"},
            "Incomplete native recipe declaration")
    require(all(recipe[key] == value for key, value in fixed_recipe.items()),
            "Profile changes original native recipe")
    inputs = source_identities(profile, root)
    inputs[str(path)] = identity(path)
    cache = (build / "CMakeCache.txt").read_text()
    inputs[str(build / "CMakeCache.txt")] = identity(build / "CMakeCache.txt")
    require("SKITY_CT_FONT:BOOL=ON" in cache, "CoreText build required")
    require("SKITY_GOLDEN_GUI:BOOL=OFF" in cache,
            "Explicit native profiles require noninteractive verification")
    producer = build / "test/golden/skity_coretext_reference"
    require(producer.is_file(), "Build skity_coretext_reference first")
    inputs[str(producer)] = identity(producer)
    native_toolchain = {}
    dependencies = native_dependencies(producer, build, inputs, native_toolchain)
    parent = build / "native_coretext_profiles" / name
    parent.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-", dir=parent)).resolve()
    coverage = directory / "coverage.a8"
    environment = native_environment()
    command = [str(producer), str(coverage)]
    result = subprocess.run(command, env=environment, capture_output=True,
                            text=True, timeout=30, check=False)
    (directory / "producer.stdout").write_text(result.stdout)
    (directory / "producer.stderr").write_text(result.stderr)
    require(result.returncode == 0,
            "Native producer refused; preserve " + str(directory))
    metadata = json.loads(result.stdout)
    inputs.update(validate_native(profile, metadata))
    for input_path, expected in inputs.items():
        require(identity(input_path) == expected,
                "Profile input changed during preparation")
    images = {}
    for filename, rgba in rgba_references(coverage.read_bytes()).items():
        output = directory / filename
        output.write_bytes(png_bytes(rgba))
        images[filename] = identity(output)
        output.chmod(0o444)
    record = {"schema_version": 1, "profile": name, "producer_command": command,
              "producer_image": inputs[str(producer)], "native": metadata,
              "inputs": inputs, "coverage": identity(coverage),
              "images": images, "producer_dependencies": dependencies,
              "native_toolchain": native_toolchain}
    (directory / "provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    return PreparedProfile(name, directory, profile, metadata, inputs, images)


def run_contract_suite(runner):
    start = time.monotonic()
    test_path = Path(runner.repo_root) / "tools/tests/coretext_profile_test.py"
    import importlib.util
    spec = importlib.util.spec_from_file_location("coretext_profile_tests", test_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    suite = unittest.defaultTestLoader.loadTestsFromModule(module)
    log = io.StringIO()
    result = unittest.TextTestRunner(stream=log, verbosity=2).run(suite)
    path = Path(runner.build_dir) / "coretext_profile_contract.log"
    path.write_text(log.getvalue())
    failures = []
    for case, error in [*result.failures, *result.errors]:
        failures.append({"suite": runner.suite, "case_name": case.id(),
                         "status": "FAIL", "exit_code": 1,
                         "stage": "Profile contract", "reason_code": "assert_failed",
                         "backend": "none", "oracle": "contract",
                         "error": error, "artifacts": {"log": str(path)}})
    if not result.testsRun or result.skipped:
        failures.append({"suite": runner.suite, "case_name": "Completeness",
                         "status": "FAIL", "exit_code": 1,
                         "stage": "Profile contract", "reason_code": "missing_cases",
                         "backend": "none", "oracle": "contract",
                         "error": "Contract cases must execute without skips",
                         "artifacts": {"log": str(path)}})
    return {"summary": {"total": result.testsRun,
                        "passed": result.testsRun - len(result.failures) - len(result.errors),
                        "failed": len(failures),
                        "duration_ms": round((time.monotonic() - start) * 1000)},
            "failures": failures}


def run_inspection_suite(runner):
    """Observe a native producer; this never admits or creates a profile."""
    start = time.monotonic()
    root, build = Path(runner.repo_root).resolve(), Path(runner.build_dir).resolve()
    producer = build / "test/golden/skity_coretext_reference"
    directory = Path(tempfile.mkdtemp(prefix="native-inspect-", dir=build))
    try:
        require(sys.platform == "darwin", "Native inspection requires macOS")
        before = identity(producer)
        source = root / "test/golden/native_reference/coretext_reference.cc"
        source_before = identity(source)
        native_inputs, native_toolchain = {}, {}
        dependencies = native_dependencies(producer, build, native_inputs, native_toolchain)
        result = subprocess.run([str(producer), str(directory / "coverage.a8")], env=native_environment(),
                                capture_output=True, text=True, timeout=30, check=False)
        (directory / "producer.stdout").write_text(result.stdout)
        (directory / "producer.stderr").write_text(result.stderr)
        require(result.returncode == 0, "Native producer failed")
        metadata = json.loads(result.stdout)
        require(metadata["producer_source_sha256"] == source_before["sha256"],
                "Native producer source is stale")
        require(all(metadata["platform"].values()), "Incomplete platform identity")
        fonts = {role: identity(font["file"]) for role, font in metadata["fonts"].items()}
        require(identity(producer) == before and identity(source) == source_before,
                "Native inputs changed during observation")
        require(all(identity(path) == expected for path, expected in native_inputs.items()),
                "Native toolchain inputs changed during observation")
        require((directory / "coverage.a8").stat().st_size == WIDTH * HEIGHT,
                "Native coverage extent mismatch")
        record = {"native": metadata, "font_files": fonts, "producer": before,
                  "source": source_before, "dependencies": dependencies,
                  "coverage": identity(directory / "coverage.a8"),
                  "native_toolchain": native_toolchain, "native_inputs": native_inputs,
                  "profile_admission": False}
        (directory / "observation.json").write_text(json.dumps(record, indent=2) + "\n")
        return {"summary": {"total": 1, "passed": 1, "failed": 0,
                            "duration_ms": round((time.monotonic() - start) * 1000)},
                "failures": [], "native_observation": str(directory / "observation.json")}
    except (OSError, ValueError, ProfileError, subprocess.SubprocessError) as error:
        return runner._create_infra_error("native_inspection_refused",
                                          str(error) + "; artifacts: " + str(directory))


def run_negative_suite(runner):
    """Require specific failures against four independently corrupted references.

    Original command bodies, product binaries and comparator remain untouched.
    Each raw FAIL stays retained; only the expected rejection is a control PASS.
    """
    start = time.monotonic()
    if runner.coretext_profile == "legacy" or runner.test_filter:
        return runner._create_infra_error("usage_error", "Negative controls require an explicit profile and no filter")
    parent = Path(tempfile.mkdtemp(prefix="coretext-negative-", dir=runner.build_dir))
    original_suite, original_filter = runner.suite, runner.test_filter
    failures, observations = [], []
    try:
        runner.suite = "golden-text"
        runner.test_filter = ":".join(CASES)
        for kind, expected_failures in NEGATIVES.items():
            output = parent / kind
            output.mkdir()
            def transform(prepared):
                coverage = (prepared.directory / "coverage.a8").read_bytes()
                for filename, rgba in negative_references(coverage, kind).items():
                    image = prepared.directory / filename
                    image.chmod(0o644)
                    image.write_bytes(png_bytes(rgba))
                    image.chmod(0o444)
            raw = runner.run_gtest_suite(transform)
            # Copy artifacts before another control can overwrite a shared path.
            for failure in raw.get("failures", []):
                for label in ("expected_image", "actual_image", "diff_pixels_file"):
                    path = failure.get("artifacts", {}).get(label)
                    if path and Path(path).is_file():
                        copy = output / failure["case_name"] / label / Path(path).name
                        copy.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(path, copy)
                        failure["artifacts"][label] = str(copy)
            (output / "agent-test-report.json").write_text(json.dumps(raw, indent=2) + "\n")
            gtest = Path(runner.build_dir) / "gtest_report_0.json"
            if gtest.is_file():
                shutil.copyfile(gtest, output / "gtest-report.json")
            pixel = [failure for failure in raw.get("failures", [])
                     if failure.get("reason_code") == "pixel_mismatch"]
            other = [failure for failure in raw.get("failures", [])
                     if failure.get("reason_code") != "pixel_mismatch"]
            specific = ({failure["case_name"] for failure in pixel} == expected_failures
                        and len(pixel) == len(expected_failures)
                        and len(other) == 1
                        and other[0].get("reason_code") == "coretext_profile_unverified"
                        and "Native reference changed during test" in other[0].get("error", ""))
            for failure in pixel:
                path = failure.get("artifacts", {}).get("diff_pixels_file")
                differences = json.loads(Path(path).read_text()) if path else []
                specific = specific and len(differences) > 50 and failure.get("metrics", {}).get("diff_pixel_count", 0) > 50
            observations.append({"control": kind, "specific_rejection": bool(specific),
                                 "raw_failure_count": len(raw.get("failures", [])),
                                 "artifacts": str(output)})
            if not specific:
                failures.extend(runner._create_infra_error("negative_oracle_failed", "Control did not produce its exact specific failures: " + kind)["failures"])
    finally:
        runner.suite, runner.test_filter = original_suite, original_filter
        runner.prepared_coretext_profile = None
    return {"summary": {"total": len(NEGATIVES), "passed": len(NEGATIVES) - len(failures),
                        "failed": len(failures), "duration_ms": round((time.monotonic() - start) * 1000)},
            "failures": failures, "negative_observations": observations,
            "positive_profile_admission": False}
