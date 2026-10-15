#define BOOST_TEST_MODULE FolderUtils Tests
#include <boost/test/included/unit_test.hpp>

#include <QUuid>

#include "sanitizeforpath.h"

// Normal module IDs (>= 6 chars) pass through unchanged
BOOST_AUTO_TEST_CASE( test_normal_moduleID )
{
	BOOST_CHECK_EQUAL(sanitizeForPath("ResistorModuleID").toStdString(), "ResistorModuleID");
	BOOST_CHECK_EQUAL(sanitizeForPath("abc123_def456_1").toStdString(), "abc123_def456_1");
	BOOST_CHECK_EQUAL(sanitizeForPath("generic_ic_dip_14_300mil").toStdString(), "generic_ic_dip_14_300mil");
}

// Path traversal with ".." is neutralized
BOOST_AUTO_TEST_CASE( test_path_traversal )
{
	// ".." gets collapsed to "_", slashes become "_"
	QString result = sanitizeForPath("../../etc/passwd");
	BOOST_CHECK(result != "../../etc/passwd");
	BOOST_CHECK(!result.contains(".."));
	BOOST_CHECK(!result.contains("/"));
	BOOST_CHECK(!result.contains("\\"));
}

// Forward and backslashes are replaced
BOOST_AUTO_TEST_CASE( test_slashes )
{
	BOOST_CHECK(!sanitizeForPath("foo/bar").contains("/"));
	BOOST_CHECK(!sanitizeForPath("foo\\bar").contains("\\"));
	BOOST_CHECK(!sanitizeForPath("C:\\Windows\\System32").contains("\\"));
}

// Windows-illegal characters are replaced
BOOST_AUTO_TEST_CASE( test_illegal_chars )
{
	QString result = sanitizeForPath("module<>:\"|?*id");
	BOOST_CHECK(!result.contains("<"));
	BOOST_CHECK(!result.contains(">"));
	BOOST_CHECK(!result.contains(":"));
	BOOST_CHECK(!result.contains("\""));
	BOOST_CHECK(!result.contains("|"));
	BOOST_CHECK(!result.contains("?"));
	BOOST_CHECK(!result.contains("*"));
	// The alphanumeric parts are preserved
	BOOST_CHECK(result.contains("module"));
	BOOST_CHECK(result.contains("id"));
}

// Control characters (< 0x20) are stripped
BOOST_AUTO_TEST_CASE( test_control_chars )
{
	QString input = QString("module") + QChar(0x01) + QChar(0x1F) + QString("ID");
	QString result = sanitizeForPath(input);
	BOOST_CHECK_EQUAL(result.toStdString(), "moduleID");
}

// Short names (< 6 chars) get a UUID prefix, which covers Windows reserved
// device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9) without a blocklist
BOOST_AUTO_TEST_CASE( test_short_names_get_uuid_prefix )
{
	QString result = sanitizeForPath("CON");
	BOOST_CHECK(result.length() > 6);
	BOOST_CHECK(result.endsWith("_CON"));

	result = sanitizeForPath("abc");
	BOOST_CHECK(result.length() > 6);
	BOOST_CHECK(result.endsWith("_abc"));

	result = sanitizeForPath("12345");
	BOOST_CHECK(result.length() > 6);
	BOOST_CHECK(result.endsWith("_12345"));
}

// Names with exactly 6 chars are not prefixed
BOOST_AUTO_TEST_CASE( test_six_char_boundary )
{
	BOOST_CHECK_EQUAL(sanitizeForPath("abcdef").toStdString(), "abcdef");
}

// Leading dots are trimmed
BOOST_AUTO_TEST_CASE( test_leading_dots )
{
	BOOST_CHECK(!sanitizeForPath(".hidden").startsWith("."));
	BOOST_CHECK(!sanitizeForPath("...test").startsWith("."));
}

// Trailing dots and spaces are trimmed
BOOST_AUTO_TEST_CASE( test_trailing_dots_spaces )
{
	BOOST_CHECK(!sanitizeForPath("test.").endsWith("."));
	BOOST_CHECK(!sanitizeForPath("test ").endsWith(" "));
	BOOST_CHECK(!sanitizeForPath("test. ").endsWith(" "));
	BOOST_CHECK(!sanitizeForPath("test. ").endsWith("."));
}

// Empty input produces a non-empty UUID-based result
BOOST_AUTO_TEST_CASE( test_empty_input )
{
	QString result = sanitizeForPath("");
	BOOST_CHECK(!result.isEmpty());
	BOOST_CHECK(result.length() >= 36);  // UUID alone is 36 chars
}

// Input that sanitizes to short (all dots/spaces) gets UUID prefix
BOOST_AUTO_TEST_CASE( test_all_dots )
{
	QString result = sanitizeForPath("...");
	BOOST_CHECK(!result.isEmpty());
	// "..." -> "_" (dot collapse) -> leading trim removes nothing -> len 1 -> UUID prefixed
	BOOST_CHECK(result.length() > 6);
	BOOST_CHECK(result.endsWith("__"));  // UUID + "_" + "_" (the collapsed dots)
}

// Short result from all illegal chars gets UUID prefix
BOOST_AUTO_TEST_CASE( test_all_illegal_chars )
{
	QString result = sanitizeForPath("///");
	BOOST_CHECK(!result.isEmpty());
	// "///" -> "___" (3 chars < 6) -> UUID prefixed
	BOOST_CHECK(result.endsWith("____"));  // UUID + "_" + "___"
	BOOST_CHECK(result.length() > 6);
}

// Long input is truncated to 200 characters
BOOST_AUTO_TEST_CASE( test_long_input )
{
	QString longInput(300, 'a');
	QString result = sanitizeForPath(longInput);
	BOOST_CHECK(result.length() <= 200);
}

// Idempotent: sanitizing an already-safe string (>= 6 chars) returns it unchanged
BOOST_AUTO_TEST_CASE( test_idempotent )
{
	QString safe = "my_module_abc123";
	BOOST_CHECK_EQUAL(sanitizeForPath(safe).toStdString(), safe.toStdString());
	// Double-sanitize produces the same result
	BOOST_CHECK_EQUAL(sanitizeForPath(sanitizeForPath(safe)).toStdString(), safe.toStdString());
}

// Realistic malicious moduleIDs
BOOST_AUTO_TEST_CASE( test_realistic_attacks )
{
	// Path traversal to escape parts directory
	QString result1 = sanitizeForPath("../../../../etc");
	BOOST_CHECK(!result1.contains("/"));
	BOOST_CHECK(!result1.contains(".."));

	// Null byte injection
	QString withNull = QString("module") + QChar(0x00) + QString("ID");
	QString result2 = sanitizeForPath(withNull);
	BOOST_CHECK(!result2.contains(QChar(0x00)));

	// Windows absolute path
	QString result3 = sanitizeForPath("C:\\Windows\\Temp");
	BOOST_CHECK(!result3.contains("\\"));
	BOOST_CHECK(!result3.contains(":"));
}
