// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
// Verify actual C++23 std::expected parser used by web auth.
#include <common/parse.hpp>

#include <cstdint>
#include <cstdio>
#include <limits>

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

	ok(positive_id("1") == 1, "minimum positive ID");
	ok(positive_id("00123") == 123, "leading zeroes accepted");
	ok(positive_id("2147483647") == std::numeric_limits<std::int32_t>::max(), "INT32_MAX accepted");
	ok(positive_id("").error() == decimal_error::empty, "empty rejected");
	ok(positive_id("0").error() == decimal_error::non_positive, "zero rejected");
	ok(positive_id("-1").error() == decimal_error::non_positive, "negative rejected");
	ok(positive_id("foo").error() == decimal_error::invalid, "letters rejected");
	ok(positive_id("1x").error() == decimal_error::invalid, "trailing text rejected");
	ok(positive_id("42 ").error() == decimal_error::invalid, "trailing whitespace rejected");
	ok(positive_id(" 42").error() == decimal_error::invalid, "leading whitespace rejected");
	ok(positive_id("+42").error() == decimal_error::invalid, "leading plus rejected");
	ok(positive_id("12.5").error() == decimal_error::invalid, "fraction rejected");
	ok(positive_id("2147483648").error() == decimal_error::out_of_range, "positive overflow rejected");
	ok(positive_id("-2147483649").error() == decimal_error::out_of_range, "negative overflow rejected");
	ok(decimal<std::int32_t>("-2147483648") == std::numeric_limits<std::int32_t>::min(), "signed minimum accepted in generic decimal parser");
	ok(decimal<std::uint32_t>("4294967295") == std::numeric_limits<std::uint32_t>::max(), "uint32 maximum accepted");
	ok(decimal<std::uint32_t>("-1").error() == decimal_error::invalid, "negative unsigned rejected");
	ok(decimal<std::uint32_t>("4294967296").error() == decimal_error::out_of_range, "unsigned overflow rejected");

	if (failures == 0)
		std::puts("C++23 EXPECTED PARSING CONTRACT PASSED");
	return failures ? 1 : 0;
}
