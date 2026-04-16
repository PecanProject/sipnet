#!/usr/bin/env python3
from __future__ import annotations

import unittest
from pathlib import Path

import sipnet_view


REPO_ROOT = Path(__file__).resolve().parent.parent
RUSSELL_1 = REPO_ROOT / "tests" / "smoke" / "russell_1" / "sipnet.out"
RUSSELL_4 = REPO_ROOT / "tests" / "smoke" / "russell_4" / "sipnet.out"


class SipnetViewTests(unittest.TestCase):
  def test_find_header_row_skips_note_lines(self) -> None:
    self.assertEqual(sipnet_view.find_header_row(RUSSELL_4), 1)

  def test_load_output_table_discovers_plot_columns(self) -> None:
    loaded = sipnet_view.load_output_table(RUSSELL_1)
    self.assertIn("plantWoodC", loaded.plot_columns)
    self.assertNotIn("year", loaded.plot_columns)
    self.assertNotIn("day", loaded.plot_columns)
    self.assertNotIn("time", loaded.plot_columns)

  def test_parse_time_range(self) -> None:
    bounds = sipnet_view.parse_time_range("2016-001-00.00,2016-002-12.00")
    self.assertLessEqual(bounds.start, bounds.end)

  def test_parse_bad_time_range_raises(self) -> None:
    with self.assertRaises(ValueError):
      sipnet_view.parse_time_range("bad-range")


if __name__ == "__main__":
  unittest.main()
