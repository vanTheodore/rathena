// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef HTTP_HPP
#define HTTP_HPP

// NB: httplib.h must come FIRST on Windows. winapi.hpp defines NONLS, which
// excludes winnls.h (MultiByteToWideChar/CP_UTF8) from <windows.h>; including
// it before httplib.h would leave httplib's u8string_to_wstring undeclared
// under MinGW. httplib itself pulls <winsock2.h> first, so order stays safe.
#include <httplib.h>

#ifdef WIN32
#include <common/winapi.hpp>
#endif

#include <string>

typedef httplib::Request Request;
typedef httplib::Response Response;

#define HANDLER_FUNC(x) void x (const Request &req, Response &res)
typedef HANDLER_FUNC((*handler_func));

// cpp-httplib >= 0.30 split multipart bodies into text fields (fields) and
// uploaded files (files); pre-split rAthena code used req.files/has_file/
// get_file_value for everything. These helpers preserve the old behavior:
// files first (binary uploads such as Img), then text fields (AID, GDID...).
inline bool web_has_form_value(const Request &req, const std::string &key) {
	return req.form.has_file(key) || req.form.has_field(key);
}

inline std::string web_get_form_value(const Request &req, const std::string &key) {
	if (req.form.has_file(key))
		return req.form.get_file(key).content;
	return req.form.get_field(key);
}


#endif
