// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef RATHENA_PARSE_HPP
#define RATHENA_PARSE_HPP

#include <charconv>
#include <concepts>
#include <cstdint>
#include <expected>
#include <string_view>
#include <system_error>

namespace rathena::parse {

enum class decimal_error {
	empty,
	invalid,
	out_of_range,
	non_positive
};

// Strict decimal input for untrusted protocol/HTTP identifiers.
// Unlike legacy a2i(), this must consume the *entire* field; neither
// trailing junk, leading whitespace nor a leading '+' is accepted.
// Do not globally replace a2i(): game database imports intentionally
// retain atoi-compatible parsing and saturation behavior.
template <std::integral T>
	requires (!std::same_as<T, bool>)
[[nodiscard]] std::expected<T, decimal_error> decimal(std::string_view input) noexcept {
	if (input.empty())
		return std::unexpected(decimal_error::empty);

	T value{};
	const auto* const first = input.data();
	const auto* const last = first + input.size();
	const auto [end, error] = std::from_chars(first, last, value, 10);

	if (error == std::errc::result_out_of_range)
		return std::unexpected(decimal_error::out_of_range);

	if (error != std::errc{} || end != last)
		return std::unexpected(decimal_error::invalid);

	return value;
}

// Authenticated account and guild IDs must be representable positive int32s.
// The web server binds both to SQLDT_INT32; no truncation or wrapping allowed.
[[nodiscard]] inline std::expected<std::int32_t, decimal_error>
positive_id(std::string_view input) noexcept {
	auto result = decimal<std::int32_t>(input);
	if (!result)
		return std::unexpected(result.error());
	if (*result <= 0)
		return std::unexpected(decimal_error::non_positive);
	return *result;
}

} // namespace rathena::parse

#endif // RATHENA_PARSE_HPP
