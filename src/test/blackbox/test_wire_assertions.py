import json
from pathlib import Path

import pytest
from wire_assertions import assert_typed_output


def _event(columns, rows):
    return json.dumps(
        {
            "status": "result_set",
            "columns": [{"name": name, "type": sql_type} for name, sql_type in columns],
            "rows": rows,
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )


def _expected_file(tmp_path: Path, *events: str) -> Path:
    path = tmp_path / "expected.typed.jsonl"
    path.write_text("".join(f"{event}\n" for event in events), encoding="utf-8")
    return path


def test_long_string_is_compared_without_display_truncation(tmp_path) -> None:
    expected = _expected_file(
        tmp_path,
        _event([("value", "string")], [["数据库\u2028aaaaaaaaaaaaa-expected"]]),
    )
    actual = _event([("value", "string")], [["数据库\u2028aaaaaaaaaaaaa-actual"]])

    with pytest.raises(AssertionError, match="row multiset"):
        assert_typed_output(actual, expected)


def test_float32_is_compared_without_six_decimal_rounding(tmp_path) -> None:
    expected = _expected_file(tmp_path, _event([("value", "float32")], [[1.0]]))
    # The next FLOAT32 after 1.0; the old display rendered both as 1.000000.
    actual = _event([("value", "float32")], [[1.0000001192092896]])

    with pytest.raises(AssertionError, match="row multiset"):
        assert_typed_output(actual, expected)


def test_null_and_schema_are_typed(tmp_path) -> None:
    expected = _expected_file(tmp_path, _event([("value", "string")], [[None]]))

    assert_typed_output(_event([("value", "string")], [[None]]), expected)
    with pytest.raises(AssertionError, match="row multiset"):
        assert_typed_output(_event([("value", "string")], [["NULL"]]), expected)
    with pytest.raises(AssertionError, match="schema differs"):
        assert_typed_output(_event([("value", "int32")], [[1]]), expected)


def test_result_boundaries_and_error_status_are_preserved(tmp_path) -> None:
    expected = _expected_file(
        tmp_path,
        _event([("id", "int32")], []),
        '{"status":"sql_error"}',
        _event([("id", "int32")], [[1]]),
    )

    assert_typed_output(
        "\n".join(
            [
                _event([("id", "int32")], []),
                '{"status":"sql_error"}',
                _event([("id", "int32")], [[1]]),
            ]
        ),
        expected,
    )
    with pytest.raises(AssertionError, match="event count"):
        assert_typed_output(_event([("id", "int32")], [[1]]), expected)


def test_sql_error_and_transaction_abort_are_distinct(tmp_path) -> None:
    expected = _expected_file(tmp_path, '{"status":"sql_error"}')

    with pytest.raises(AssertionError, match="status differs"):
        assert_typed_output('{"status":"transaction_abort"}', expected)


def test_rows_are_a_multiset_unless_order_is_requested(tmp_path) -> None:
    expected = _expected_file(tmp_path, _event([("id", "int32")], [[1], [2]]))
    actual = _event([("id", "int32")], [[2], [1]])

    assert_typed_output(actual, expected)
    with pytest.raises(AssertionError, match="rows differ"):
        assert_typed_output(actual, expected, ordered_rows=True)


def test_runtime_oracle_must_be_typed_jsonl(tmp_path) -> None:
    legacy = tmp_path / "expected.txt"
    legacy.write_text("| value |\n| 1 |\n", encoding="utf-8")

    with pytest.raises(ValueError, match="explicit .typed.jsonl"):
        assert_typed_output(_event([("value", "int32")], [[1]]), legacy)
