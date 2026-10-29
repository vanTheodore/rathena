// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "auth.hpp"

#include <cstring>

#include <common/parse.hpp>
#include <common/showmsg.hpp>
#include <common/sql.hpp>

#include "http.hpp"
#include "sqllock.hpp"
#include "web.hpp"


bool isAuthorized(const Request &request, bool checkGuildLeader) {
	if (!web_has_form_value(request, "AuthToken") || !web_has_form_value(request, "AID"))
		return false;

	if (checkGuildLeader && !web_has_form_value(request, "GDID"))
		return false;
	
	auto token_str = web_get_form_value(request, "AuthToken");
	auto token = token_str.c_str();
	// C++23 expected: reject malformed/overflowing identifiers before SQL.
	// a2i() is intentionally permissive for legacy data imports; an HTTP
	// authorization identifier must not silently turn into zero or clamp.
	const auto parsed_account_id = rathena::parse::positive_id(web_get_form_value(request, "AID"));
	if (!parsed_account_id)
		return false;
	auto account_id = *parsed_account_id;

	SQLLock loginlock(LOGIN_SQL_LOCK);

	loginlock.lock();

	auto handle = loginlock.getHandle();

	SqlStmt stmt{ *handle };

	if (SQL_SUCCESS != stmt.Prepare(
			"SELECT `account_id` FROM `%s` WHERE (`account_id` = ? AND `web_auth_token` = ? AND `web_auth_token_enabled` = '1')",
			login_table)
		|| SQL_SUCCESS != stmt.BindParam(0, SQLDT_INT32, &account_id, sizeof(account_id))
		|| SQL_SUCCESS != stmt.BindParam(1, SQLDT_STRING, (void *)token, strlen(token))
		|| SQL_SUCCESS != stmt.Execute()
	) {
		SqlStmt_ShowDebug(stmt);
		loginlock.unlock();
		return false;
	}

	if (stmt.NumRows() <= 0) {
		ShowWarning("Request with AID %d and token %s unverified\n", account_id, token);
		loginlock.unlock();
		return false;
	}

	loginlock.unlock();
	if (!checkGuildLeader) {
		// we're done, auth ok
		return true;
	}

	const auto parsed_guild_id = rathena::parse::positive_id(web_get_form_value(request, "GDID"));
	if (!parsed_guild_id)
		return false;
	auto guild_id = *parsed_guild_id;

	SQLLock charlock(CHAR_SQL_LOCK);
	charlock.lock();
	handle = charlock.getHandle();
	SqlStmt stmt2{ *handle };

	if (SQL_SUCCESS != stmt2.Prepare(
		"SELECT `account_id` FROM `%s` LEFT JOIN `%s` using (`char_id`) WHERE (`%s`.`account_id` = ? AND `%s`.`guild_id` = ?) LIMIT 1",
		guild_db_table, char_db_table, char_db_table, guild_db_table)
		|| SQL_SUCCESS != stmt2.BindParam(0, SQLDT_INT32, &account_id, sizeof(account_id))
		|| SQL_SUCCESS != stmt2.BindParam(1, SQLDT_INT32, &guild_id, sizeof(guild_id))
		|| SQL_SUCCESS != stmt2.Execute()
	) {
		SqlStmt_ShowDebug(stmt2);
		charlock.unlock();
		return false;
	}

	if (stmt2.NumRows() <= 0) {
		ShowDebug("Request with AID %d GDID %d and token %s unverified\n", account_id, guild_id, token);
		charlock.unlock();
		return false;
	}
	charlock.unlock();
	return true;
}
