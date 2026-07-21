#!/usr/bin/env python3
from __future__ import annotations

import unittest
from pathlib import Path

import tools.sipnet_debug_view as sipnet_debug_view



REPO_ROOT = Path(__file__).resolve().parent.parent
SAMPLE_DEBUG_PREFIX = REPO_ROOT / "tests" / "data" / "debug" / "sample"


class SipnetDebugViewTests(unittest.TestCase):
  def test_load_debug_tables_returns_merged_frame(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertIsInstance(loaded, sipnet_debug_view.LoadedSipnetData)
    self.assertGreater(len(loaded.frame), 0)

  def test_load_debug_tables_prefixes_envi_columns(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertIn("envi.plantWoodC", loaded.plot_columns)
    self.assertIn("envi.soilC", loaded.plot_columns)

  def test_load_debug_tables_prefixes_flux_columns(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertIn("flux.photosynthesis", loaded.plot_columns)
    self.assertIn("flux.rVeg", loaded.plot_columns)

  def test_load_debug_tables_prefixes_tracker_columns(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertIn("tracker.t.gpp", loaded.plot_columns)
    self.assertIn("tracker.pt.didLeafGrowth", loaded.plot_columns)

  def test_load_debug_tables_excludes_time_columns_from_plot_columns(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertNotIn("year", loaded.plot_columns)
    self.assertNotIn("day", loaded.plot_columns)
    self.assertNotIn("time", loaded.plot_columns)

  def test_load_debug_tables_missing_file_raises(self) -> None:
    missing_prefix = REPO_ROOT / "tests" / "data" / "debug" / "nonexistent"
    with self.assertRaises(ValueError):
      sipnet_debug_view.load_debug_tables(missing_prefix)

  def test_derive_prefix_from_envi_log(self) -> None:
    path = Path("/path/to/run_envi.log")
    self.assertEqual(
      sipnet_debug_view.derive_prefix_from_log_file(path),
      "/path/to/run",
    )

  def test_derive_prefix_from_fluxes_log(self) -> None:
    path = Path("/path/to/run_fluxes.log")
    self.assertEqual(
      sipnet_debug_view.derive_prefix_from_log_file(path),
      "/path/to/run",
    )

  def test_derive_prefix_from_trackers_log(self) -> None:
    path = Path("/path/to/run_trackers.log")
    self.assertEqual(
      sipnet_debug_view.derive_prefix_from_log_file(path),
      "/path/to/run",
    )

  def test_derive_prefix_from_unknown_file_returns_path_unchanged(self) -> None:
    path = Path("/path/to/something.txt")
    self.assertEqual(
      sipnet_debug_view.derive_prefix_from_log_file(path),
      "/path/to/something.txt",
    )

  def test_load_debug_tables_row_count_matches_data_files(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    # All sample files have 4 data rows
    self.assertEqual(len(loaded.frame), 4)

  def test_load_debug_tables_path_is_prefix(self) -> None:
    loaded = sipnet_debug_view.load_debug_tables(SAMPLE_DEBUG_PREFIX)
    self.assertEqual(loaded.path, SAMPLE_DEBUG_PREFIX)


if __name__ == "__main__":
  unittest.main()
