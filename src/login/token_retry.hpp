// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef TOKEN_RETRY_HPP
#define TOKEN_RETRY_HPP

#include <common/cbasetypes.hpp>
#include <common/sql.hpp> // SQL_SUCCESS

/// Runs a token refresh query until it succeeds, fails with a non-duplicate
/// error, or exhausts max_retries attempts.
/// run_query() performs one attempt and returns SQL_SUCCESS/SQL_ERROR;
/// last_errno() reports the error code of the last attempt (1062 on token
/// collision: the statement text is re-run unchanged and the server draws
/// fresh randomness per execution, so a retry is a new independent draw).
/// Returns true on success; attempts_out (nullable) receives the number of
/// attempts performed: at most max_retries, exactly 1 on first-try success
/// or on a non-retryable error.
template<typename QueryFn, typename ErrnoFn>
inline bool mmo_auth_token_retry(QueryFn&& run_query, ErrnoFn&& last_errno, int32 max_retries, int32* attempts_out){
	int32 attempts = 0;
	bool success = false;
	while( attempts < max_retries ){
		++attempts;
		if( run_query() == SQL_SUCCESS ){
			success = true;
			break;
		}
		if( last_errno() != 1062 )
			break; // not a token collision: retrying cannot help
	}
	if( attempts_out != nullptr )
		*attempts_out = attempts;
	return success;
}

#endif /* TOKEN_RETRY_HPP */
