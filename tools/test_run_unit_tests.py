#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPT_PATH = REPO_ROOT / "tools" / "run_unit_tests.sh"


class RunUnitTestsWrapperTests(unittest.TestCase):
  def _run_wrapper_in_dir(self, test_dir: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["bash", str(SCRIPT_PATH)],
        cwd=test_dir,
        capture_output=True,
        text=True,
        check=False,
    )

  def test_all_passing_tests_exit_zero(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      root = Path(tmpdir)
      suite_dir = root / "tests" / "sipnet" / "suite"
      suite_dir.mkdir(parents=True)

      (root / "Makefile").write_text(".PHONY: testbuild\ntestbuild:\n\t@:\n", encoding="utf-8")
      (suite_dir / "Makefile").write_text("TEST_CFILES = pass.c\n.PHONY: all\nall:\n\t@:\n", encoding="utf-8")

      pass_exe = suite_dir / "pass"
      pass_exe.write_text("#!/bin/sh\necho test passed\nexit 0\n", encoding="utf-8")
      pass_exe.chmod(0o755)

      proc = self._run_wrapper_in_dir(root)
      self.assertEqual(proc.returncode, 0)
      self.assertIn("PASS", proc.stdout)

  def test_failing_test_exits_nonzero(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      root = Path(tmpdir)
      suite_dir = root / "tests" / "sipnet" / "suite"
      suite_dir.mkdir(parents=True)

      (root / "Makefile").write_text(".PHONY: testbuild\ntestbuild:\n\t@:\n", encoding="utf-8")
      (suite_dir / "Makefile").write_text("TEST_CFILES = fail.c\n.PHONY: all\nall:\n\t@:\n", encoding="utf-8")

      fail_exe = suite_dir / "fail"
      fail_exe.write_text("#!/bin/sh\necho test failed\nexit 5\n", encoding="utf-8")
      fail_exe.chmod(0o755)

      proc = self._run_wrapper_in_dir(root)
      self.assertEqual(proc.returncode, 1)
      self.assertIn("FAIL (5)", proc.stdout)

  def test_missing_executable_exits_nonzero(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      root = Path(tmpdir)
      suite_dir = root / "tests" / "sipnet" / "suite"
      suite_dir.mkdir(parents=True)

      (root / "Makefile").write_text(".PHONY: testbuild\ntestbuild:\n\t@:\n", encoding="utf-8")
      (suite_dir / "Makefile").write_text("TEST_CFILES = missing.c\n.PHONY: all\nall:\n\t@:\n", encoding="utf-8")

      proc = self._run_wrapper_in_dir(root)
      self.assertEqual(proc.returncode, 1)
      self.assertIn("MISSING", proc.stdout)

  def test_continued_execution_after_failure(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      root = Path(tmpdir)
      suite_dir = root / "tests" / "sipnet" / "suite"
      suite_dir.mkdir(parents=True)

      (root / "Makefile").write_text(".PHONY: testbuild\ntestbuild:\n\t@:\n", encoding="utf-8")
      (suite_dir / "Makefile").write_text("TEST_CFILES = first.c second.c\n.PHONY: all\nall:\n\t@:\n", encoding="utf-8")

      first_exe = suite_dir / "first"
      first_exe.write_text("#!/bin/sh\necho fail first\nexit 3\n", encoding="utf-8")
      first_exe.chmod(0o755)

      second_exe = suite_dir / "second"
      second_exe.write_text("#!/bin/sh\necho pass second\nexit 0\n", encoding="utf-8")
      second_exe.chmod(0o755)

      proc = self._run_wrapper_in_dir(root)
      self.assertEqual(proc.returncode, 1)
      self.assertIn("FAIL (3)", proc.stdout)
      self.assertIn("PASS", proc.stdout)


if __name__ == "__main__":
  unittest.main()
