// Copyright (c) 2026, DanketSub contributors
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/// @file test_support.h
/// @brief Shared assertion helpers for the Qt client tests

#pragma once

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <cstdio>
#include <string>

namespace danketsub_test {

inline int failures = 0;

inline void report_failure(char const *file, int line, std::string const &message) {
	std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, message.c_str());
	++failures;
}

inline std::string describe_color(QColor const &color) {
	return color.isValid() ? color.name(QColor::HexArgb).toStdString() : std::string("<invalid>");
}

inline void check_true(char const *expression, bool actual, char const *file, int line) {
	if (!actual)
		report_failure(file, line, std::string(expression) + " was false");
}

inline void check_equal_int(char const *expression, int actual, int expected, char const *file, int line) {
	if (actual != expected)
		report_failure(file, line, std::string(expression) + ": expected " + std::to_string(expected) +
		                                ", got " + std::to_string(actual));
}

inline void check_string(char const *expression, QString const &actual, QString const &expected, char const *file, int line) {
	if (actual != expected)
		report_failure(file, line, std::string(expression) + ": expected '" + expected.toStdString() +
		                                "', got '" + actual.toStdString() + "'");
}

inline void check_color(char const *expression, QColor const &actual, QColor const &expected, char const *file, int line) {
	if (actual != expected)
		report_failure(file, line, std::string(expression) + ": expected " + describe_color(expected) +
		                                ", got " + describe_color(actual));
}

inline QString write_palette(QTemporaryDir const &dir, QString const &name, QByteArray const &contents) {
	QString const path = dir.filePath(name);
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return {};
	file.write(contents);
	return path;
}

} // namespace danketsub_test

#define CHECK_TRUE(condition) ::danketsub_test::check_true(#condition, (condition), __FILE__, __LINE__)
#define CHECK_EQ_INT(actual, expected) \
	::danketsub_test::check_equal_int(#actual, static_cast<int>(actual), static_cast<int>(expected), __FILE__, __LINE__)
#define CHECK_STRING(actual, expected) \
	::danketsub_test::check_string(#actual, (actual), (expected), __FILE__, __LINE__)
#define CHECK_COLOR(actual, expected) \
	::danketsub_test::check_color(#actual, (actual), (expected), __FILE__, __LINE__)
