// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
// Verify actual C++23 std::expected parser used by web auth.
#include <common/parse.hpp>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

using rathena::parse::decimal;
using rathena::parse::decimal_error;
using rathena::parse::positive_id;

int main() {
	int failures = 0;
	const auto ok = [&failures](bool cond, const char* name) {
		if (!cond) {
			std::printf("FAIL: %s\n", name);
			++failures;
		}
	};
	const auto expect_id_error = [&ok](std::string_view text, decimal_error error, const char* name) {
		const auto result = positive_id(text);
		ok(!result && result.error() == error, name);
	};

	ok(positive_id("1") == 1, "minimum positive ID");
	ok(positive_id("00123") == 123, "leading zeroes accepted");
	ok(positive_id("2147483647") == std::numeric_limits<std::int32_t>::max(), "INT32_MAX accepted");
	expect_id_error("", decimal_error::empty, "empty rejected");
	expect_id_error("0", decimal_error::non_positive, "zero rejected");
	expect_id_error("-1", decimal_error::non_positive, "negative rejected");
	expect_id_error("foo", decimal_error::invalid, "letters rejected");
	expect_id_error("1x", decimal_error::invalid, "trailing text rejected");
	expect_id_error("42 ", decimal_error::invalid, "trailing whitespace rejected");
	expect_id_error(" 42", decimal_error::invalid, "leading whitespace rejected");
	expect_id_error("+42", decimal_error::invalid, "leading plus rejected");
	expect_id_error("12.5", decimal_error::invalid, "fraction rejected");
	expect_id_error("2147483648", decimal_error::out_of_range, "positive overflow rejected");
	expect_id_error("-2147483649", decimal_error::out_of_range, "negative overflow rejected");
	ok(decimal<std::int32_t>("-2147483648") == std::numeric_limits<std::int32_t>::min(), "signed minimum accepted in generic decimal parser");
	ok(decimal<std::uint32_t>("4294967295") == std::numeric_limits<std::uint32_t>::max(), "uint32 maximum accepted");
	{
		const auto result = decimal<std::uint32_t>("-1");
		ok(!result && result.error() == decimal_error::invalid, "negative unsigned rejected");
	}
	{
		const auto result = decimal<std::uint32_t>("4294967296");
		ok(!result && result.error() == decimal_error::out_of_range, "unsigned overflow rejected");
	}

	if (failures == 0)
		std::puts("C++23 EXPECTED PARSING CONTRACT PASSED");
	return failures ? 1 : 0;
}
