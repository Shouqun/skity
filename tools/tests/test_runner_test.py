# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Standalone process/report consistency regressions for the unified runner."""
import argparse
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

_SPEC = importlib.util.spec_from_file_location(
    "skity_runner_under_test", Path(__file__).resolve().parents[1] / "test-runner.py")
runner_module = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(runner_module)


class RunnerProcessContract(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="skity_runner_contract_")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        args = argparse.Namespace(
            build_dir=str(self.root / "build"), parallel=1, filter="", verbose=False,
            clean=False, no_color=True, no_configure=True, no_build=True,
            run_only=True, suite="unit", backend="metal", coretext_profile="legacy")
        self.runner = runner_module.TestRunner(args)
        self.report = self.root / "complete.json"

    @staticmethod
    def completed(assertion=False):
        case = {"name": "Completed", "status": "RUN", "result": "COMPLETED"}
        if assertion:
            case["failures"] = [{"failure": "fixture.cc:12\noriginal assertion"}]
        return {"tests": 1, "failures": int(assertion),
                "testsuites": [{"name": "Fixture", "testsuite": [case]}]}

    def parse(self, code, data=None):
        data = self.completed() if data is None else data
        text = json.dumps(data)
        self.report.write_text(text)
        result = self.runner._parse_gtest_json(str(self.report), code, "original stdout")
        self.assertEqual(self.report.read_text(), text)
        return result

    def assert_process_failure(self, result, code):
        self.assertGreater(result["summary"]["failed"], 0)
        self.assertEqual(len(result["failures"]), 1)
        failure = result["failures"][0]
        self.assertEqual(failure["reason_code"], "runtime_nonzero_exit")
        self.assertEqual(failure["actual_exit_code"], code)
        self.assertEqual(failure["exit_code"], runner_module.EXIT_INFRA_ERROR)

    def pipeline(self, code, write_report=True, assertion=False):
        def execute(command, **kwargs):
            report = next(a.split("json:", 1)[1] for a in command
                          if a.startswith("--gtest_output=json:"))
            if write_report:
                Path(report).write_text(json.dumps(self.completed(assertion)))
            return subprocess.CompletedProcess(command, code, "original stdout", "original stderr")
        with patch.object(self.runner, "find_test_executables", return_value=["skity_unit_test"]), \
             patch.object(self.runner, "prepare_test_environment", return_value=(dict(os.environ), None)), \
             patch.object(runner_module.subprocess, "run", side_effect=execute), \
             contextlib.redirect_stdout(io.StringIO()):
            status = self.runner.run()
        return status, json.loads(Path(self.runner.report_file).read_text())

    def test_success_report_and_zero_exit_preserve_success(self):
        result = self.parse(0)
        self.assertEqual(result["summary"]["total"], 1)
        self.assertEqual(result["summary"]["passed"], 1)
        self.assertEqual(result["failures"], [])

    def test_success_report_and_nonzero_exit_fail(self):
        result = self.parse(73)
        self.assert_process_failure(result, 73)
        self.assertEqual(result["summary"]["passed"], 1)
        self.assertEqual(result["summary"]["total"], 2)

    def test_completed_report_and_signal_status_fail(self):
        self.assert_process_failure(self.parse(-15), -15)

    def test_empty_report_does_not_mask_nonzero_exit(self):
        self.assert_process_failure(self.parse(73, {"tests": 0, "failures": 0, "testsuites": []}), 73)

    def test_original_assertion_failure_is_not_duplicated(self):
        result = self.parse(1, self.completed(assertion=True))
        self.assertEqual(result["summary"]["total"], 1)
        self.assertEqual(result["summary"]["failed"], 1)
        self.assertEqual(len(result["failures"]), 1)
        self.assertEqual(result["failures"][0]["reason_code"], "assert_failed")
        self.assertIn("original assertion", result["failures"][0]["error"])

    def test_success_json_nonzero_process_makes_cli_fail(self):
        status, result = self.pipeline(73)
        self.assertEqual(status, runner_module.EXIT_INFRA_ERROR)
        self.assert_process_failure(result, 73)

    def test_missing_report_nonzero_process_keeps_crash_failure(self):
        status, result = self.pipeline(73, write_report=False)
        self.assertEqual(status, runner_module.EXIT_INFRA_ERROR)
        self.assertEqual(result["failures"][0]["reason_code"], "runtime_crash")
        self.assertIn("73", result["failures"][0]["error"])

    def test_missing_report_zero_process_keeps_report_failure(self):
        status, result = self.pipeline(0, write_report=False)
        self.assertEqual(status, runner_module.EXIT_INFRA_ERROR)
        self.assertEqual(result["failures"][0]["reason_code"], "missing_gtest_report")

    def test_assertion_process_keeps_test_failure_exit(self):
        status, result = self.pipeline(1, assertion=True)
        self.assertEqual(status, runner_module.EXIT_TEST_FAILED)
        self.assertEqual(result["failures"][0]["reason_code"], "assert_failed")

    def real_process_report(self, code):
        program = ("from pathlib import Path; import sys; "
                   "Path(sys.argv[1]).write_text(sys.argv[2]); "
                   "raise SystemExit(int(sys.argv[3]))")
        completed = subprocess.run([sys.executable, "-c", program, str(self.report),
                                    json.dumps(self.completed()), str(code)],
                                   capture_output=True, text=True, check=False)
        self.assertEqual(completed.returncode, code)
        return self.runner._parse_gtest_json(str(self.report), completed.returncode, completed.stdout)

    def test_real_completed_process_nonzero_exit_is_retained(self):
        self.assert_process_failure(self.real_process_report(73), 73)

    def test_real_completed_process_zero_exit_succeeds(self):
        self.assertEqual(self.real_process_report(0)["failures"], [])
