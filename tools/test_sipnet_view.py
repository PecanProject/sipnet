#!/usr/bin/env python3
from __future__ import annotations

import unittest
from pathlib import Path

import tools.sipnet_view as sipnet_view


REPO_ROOT = Path(__file__).resolve().parent.parent
RUSSELL_1_OUTPUT = REPO_ROOT / "tests" / "smoke" / "russell_1" / "sipnet.out"
RUSSELL_1_EVENTS = REPO_ROOT / "tests" / "smoke" / "russell_1" / "events.out"
RUSSELL_4_OUTPUT = REPO_ROOT / "tests" / "smoke" / "russell_4" / "sipnet.out"


class SipnetViewTests(unittest.TestCase):
  def test_find_output_header_row_skips_note_lines(self) -> None:
    self.assertEqual(sipnet_view.find_output_header_row(RUSSELL_4_OUTPUT), 1)

  def test_load_output_table_discovers_plot_columns(self) -> None:
    loaded = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)
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

  def test_default_events_path(self) -> None:
    expected = RUSSELL_1_OUTPUT.with_name("events.out")
    self.assertEqual(sipnet_view.default_events_path(RUSSELL_1_OUTPUT), expected)

  def test_load_events_table_discovers_event_types(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)
    loaded_events = sipnet_view.load_events_table(RUSSELL_1_EVENTS, loaded_output)
    self.assertIn("leafon", loaded_events.event_types)
    self.assertIn("irrig", loaded_events.event_types)

  def test_add_derived_column_appends_plot_column(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    new_name = sipnet_view.add_derived_column(
      loaded_output,
      "wood_plus_leaf",
      "plantWoodC + plantLeafC",
    )

    self.assertEqual(new_name, "wood_plus_leaf")
    self.assertIn("wood_plus_leaf", loaded_output.plot_columns)
    self.assertTrue(
      (
        loaded_output.frame["wood_plus_leaf"]
        == loaded_output.frame["plantWoodC"] + loaded_output.frame["plantLeafC"]
      ).all()
    )

  def test_add_derived_column_can_reference_earlier_added_column(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    sipnet_view.add_derived_column(
      loaded_output,
      "wood_plus_leaf",
      "plantWoodC + plantLeafC",
    )
    sipnet_view.add_derived_column(
      loaded_output,
      "double_wood_plus_leaf",
      "wood_plus_leaf * 2",
    )

    self.assertTrue(
      (
        loaded_output.frame["double_wood_plus_leaf"]
        == loaded_output.frame["wood_plus_leaf"] * 2
      ).all()
    )

  def test_add_derived_column_rejects_invalid_name(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    with self.assertRaisesRegex(ValueError, "ASCII letters, digits, and underscores"):
      sipnet_view.add_derived_column(
        loaded_output,
        "1bad-name",
        "plantWoodC + plantLeafC",
      )

  def test_add_derived_column_rejects_python_keyword_name(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    with self.assertRaisesRegex(ValueError, "Python keywords are not allowed"):
      sipnet_view.add_derived_column(
        loaded_output,
        "class",
        "plantWoodC + plantLeafC",
      )

  def test_add_derived_column_rejects_unknown_expression_name(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    with self.assertRaisesRegex(ValueError, "Failed to evaluate expression"):
      sipnet_view.add_derived_column(
        loaded_output,
        "bad_expr",
        "plantWoodC + missing_column",
      )

  def test_add_derived_column_rejects_assignment_expression(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)

    with self.assertRaisesRegex(ValueError, "Series or scalar, not multiple columns"):
      sipnet_view.add_derived_column(
        loaded_output,
        "bad_assignment",
        "assigned = plantWoodC + plantLeafC",
      )

    self.assertNotIn("assigned", loaded_output.frame.columns)

  def test_event_headers_require_year_day_type(self) -> None:
    self.assertEqual(sipnet_view.find_events_header_row(RUSSELL_1_EVENTS), 0)

  def test_event_timestamp_mapping_uses_output_day_when_available(self) -> None:
    loaded_output = sipnet_view.load_output_table(RUSSELL_1_OUTPUT)
    loaded_events = sipnet_view.load_events_table(RUSSELL_1_EVENTS, loaded_output)

    leafon = loaded_events.frame.iloc[0]
    year_column, day_column, _ = loaded_output.time_columns
    matching = loaded_output.frame.loc[
      (loaded_output.frame[year_column] == leafon["year"])
      & (loaded_output.frame[day_column] == leafon["day"])
      ]

    self.assertFalse(matching.empty)
    self.assertEqual(
      leafon[sipnet_view.EVENT_TIMESTAMP_COLUMN],
      matching.iloc[0][sipnet_view.INTERNAL_TIMESTAMP_COLUMN],
    )


if __name__ == "__main__":
  unittest.main()
