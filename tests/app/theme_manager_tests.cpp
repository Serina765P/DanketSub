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

/// @file theme_manager_tests.cpp
/// @brief Self-contained tests for the palette resolution and JSON fallback

#include "theme_manager.h"
#include "test_support.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include <iostream>
#include <vector>

using namespace danketsub_test;

namespace {

using danket::ui::ThemeManager;

struct Recorder {
	int paletteChanges = 0;
	int modeChanges = 0;
	int errorChanges = 0;

	explicit Recorder(ThemeManager &theme) {
		QObject::connect(&theme, &ThemeManager::paletteChanged, &theme, [this] { ++paletteChanges; });
		QObject::connect(&theme, &ThemeManager::modeChanged, &theme, [this] { ++modeChanges; });
		QObject::connect(&theme, &ThemeManager::lastErrorChanged, &theme, [this] { ++errorChanges; });
	}
};

QColor builtin(bool dark, char const *key) {
	ThemeManager reference;
	reference.setMode(dark ? ThemeManager::Mode::Dark : ThemeManager::Mode::Light);
	return reference.color(QString::fromLatin1(key));
}

void test_builtin_palette_is_complete() {
	ThemeManager lightTheme;
	lightTheme.setMode(ThemeManager::Mode::Light);
	ThemeManager darkTheme;
	darkTheme.setMode(ThemeManager::Mode::Dark);

	QStringList const keys = ThemeManager::paletteKeys();
	QVariantMap const light = lightTheme.palette();
	QVariantMap const dark = darkTheme.palette();

	CHECK_EQ_INT(light.size(), keys.size());
	CHECK_EQ_INT(dark.size(), keys.size());
	for (QString const &key : keys) {
		CHECK_TRUE(light.value(key).value<QColor>().isValid());
		CHECK_TRUE(dark.value(key).value<QColor>().isValid());
	}

	CHECK_TRUE(light.value("windowBg").value<QColor>() != dark.value("windowBg").value<QColor>());
	CHECK_TRUE(light.value("waveformBg").value<QColor>() != dark.value("waveformBg").value<QColor>());
}

void test_system_mode_defaults_to_light_without_a_gui_application() {
	ThemeManager theme;
	CHECK_TRUE(theme.mode() == ThemeManager::Mode::System);
	CHECK_TRUE(!theme.isDark());
	CHECK_COLOR(theme.color("windowBg"), builtin(false, "windowBg"));
	CHECK_COLOR(theme.color("gridZebraBg"), builtin(false, "gridZebraBg"));
}

void test_mode_switch_notifies_and_resolves_palette() {
	ThemeManager theme;
	Recorder recorder(theme);

	theme.setMode(ThemeManager::Mode::Dark);
	CHECK_TRUE(theme.isDark());
	CHECK_EQ_INT(recorder.modeChanges, 1);
	CHECK_EQ_INT(recorder.paletteChanges, 1);
	CHECK_COLOR(theme.color("windowBg"), builtin(true, "windowBg"));

	theme.setMode(ThemeManager::Mode::Light);
	CHECK_TRUE(!theme.isDark());
	CHECK_EQ_INT(recorder.modeChanges, 2);
	CHECK_EQ_INT(recorder.paletteChanges, 2);
	CHECK_COLOR(theme.color("windowBg"), builtin(false, "windowBg"));

	theme.setMode(ThemeManager::Mode::Light);
	CHECK_EQ_INT(recorder.modeChanges, 2);
	CHECK_EQ_INT(recorder.paletteChanges, 2);

	theme.setMode(ThemeManager::Mode::System);
	CHECK_TRUE(!theme.isDark());
	CHECK_EQ_INT(recorder.modeChanges, 3);
	CHECK_EQ_INT(recorder.paletteChanges, 3);
}

void test_loads_mode_specific_and_shared_colors() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const path = write_palette(dir, "custom.json", R"({
		"name": "Test Palette",
		"accent": "#abcdef",
		"light": { "windowBg": "#112233", "gridZebraBg": "#e1e2e3" },
		"dark": { "windowBg": "#010203" }
	})");
	CHECK_TRUE(!path.isEmpty());

	ThemeManager theme;
	Recorder recorder(theme);

	CHECK_TRUE(theme.loadPalette(path));
	CHECK_STRING(theme.paletteName(), QStringLiteral("Test Palette"));
	CHECK_STRING(theme.palettePath(), QFileInfo(path).absoluteFilePath());
	CHECK_TRUE(theme.lastError().isEmpty());
	CHECK_EQ_INT(recorder.paletteChanges, 1);

	theme.setMode(ThemeManager::Mode::Light);
	CHECK_COLOR(theme.color("windowBg"), QColor("#112233"));
	CHECK_COLOR(theme.color("gridZebraBg"), QColor("#e1e2e3"));
	CHECK_COLOR(theme.color("accent"), QColor("#abcdef"));
	CHECK_COLOR(theme.color("textPrimary"), builtin(false, "textPrimary"));

	theme.setMode(ThemeManager::Mode::Dark);
	CHECK_COLOR(theme.color("windowBg"), QColor("#010203"));
	CHECK_COLOR(theme.color("accent"), QColor("#abcdef"));
	CHECK_COLOR(theme.color("gridZebraBg"), builtin(true, "gridZebraBg"));
}

void test_unknown_and_invalid_entries_fall_back_per_key() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const path = write_palette(dir, "partial.json", R"({
		"name": "Partial",
		"light": {
			"unknownKey": "#123456",
			"windowBg": "not-a-colour",
			"textPrimary": "#112233"
		}
	})");
	CHECK_TRUE(!path.isEmpty());

	ThemeManager theme;
	theme.setMode(ThemeManager::Mode::Light);
	CHECK_TRUE(theme.loadPalette(path));
	CHECK_TRUE(!theme.color("unknownKey").isValid());
	CHECK_COLOR(theme.color("windowBg"), builtin(false, "windowBg"));
	CHECK_COLOR(theme.color("textPrimary"), QColor("#112233"));
}

void test_malformed_palette_keeps_previous_theme() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const goodPath = write_palette(dir, "good.json", R"({
		"name": "Good",
		"light": { "windowBg": "#112233" }
	})");
	QString const brokenPath = write_palette(dir, "broken.json", R"({ "light": { "windowBg": })");
	QString const wrongShapePath = write_palette(dir, "array.json", R"([ "not", "an", "object" ])");
	QString const badSectionPath = write_palette(dir, "section.json", R"({ "light": [] })");
	CHECK_TRUE(!goodPath.isEmpty() && !brokenPath.isEmpty() && !wrongShapePath.isEmpty() && !badSectionPath.isEmpty());

	ThemeManager theme;
	theme.setMode(ThemeManager::Mode::Light);
	CHECK_TRUE(theme.loadPalette(goodPath));
	QColor const loaded = theme.color("windowBg");
	Recorder recorder(theme);

	CHECK_TRUE(!theme.loadPalette(brokenPath));
	CHECK_TRUE(!theme.lastError().isEmpty());
	CHECK_COLOR(theme.color("windowBg"), loaded);
	CHECK_STRING(theme.paletteName(), QStringLiteral("Good"));
	CHECK_EQ_INT(recorder.paletteChanges, 0);
	CHECK_EQ_INT(recorder.errorChanges, 1);

	CHECK_TRUE(!theme.loadPalette(wrongShapePath));
	CHECK_TRUE(!theme.lastError().isEmpty());
	CHECK_COLOR(theme.color("windowBg"), loaded);

	CHECK_TRUE(!theme.loadPalette(badSectionPath));
	CHECK_TRUE(!theme.lastError().isEmpty());
	CHECK_COLOR(theme.color("windowBg"), loaded);
}

void test_missing_file_reports_error() {
	ThemeManager theme;
	Recorder recorder(theme);
	CHECK_TRUE(!theme.loadPalette(QDir::temp().filePath(QStringLiteral("danketsub-missing-palette.json"))));
	CHECK_TRUE(!theme.lastError().isEmpty());
	CHECK_EQ_INT(recorder.errorChanges, 1);
	CHECK_TRUE(theme.palettePath().isEmpty());
}

void test_file_url_is_accepted() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const path = write_palette(dir, "url.json", R"({ "dark": { "cursorColor": "#334455" } })");
	CHECK_TRUE(!path.isEmpty());

	ThemeManager theme;
	CHECK_TRUE(theme.loadPalette(QUrl::fromLocalFile(path).toString()));
	theme.setMode(ThemeManager::Mode::Dark);
	CHECK_COLOR(theme.color("cursorColor"), QColor("#334455"));
	CHECK_STRING(theme.paletteName(), QStringLiteral("url"));
}

void test_reset_restores_builtin_palette() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const path = write_palette(dir, "reset.json", R"({
		"name": "Reset Me",
		"light": { "windowBg": "#112233" }
	})");
	CHECK_TRUE(!path.isEmpty());

	ThemeManager theme;
	theme.setMode(ThemeManager::Mode::Light);
	CHECK_TRUE(theme.loadPalette(path));
	CHECK_COLOR(theme.color("windowBg"), QColor("#112233"));

	Recorder recorder(theme);
	theme.resetPalette();
	CHECK_EQ_INT(recorder.paletteChanges, 1);
	CHECK_TRUE(theme.palettePath().isEmpty());
	CHECK_TRUE(theme.paletteName().isEmpty());
	CHECK_COLOR(theme.color("windowBg"), builtin(false, "windowBg"));

	theme.resetPalette();
	CHECK_EQ_INT(recorder.paletteChanges, 1);
}

struct TestCase {
	char const *name;
	void (*run)();
};

std::vector<TestCase> const &all_tests() {
	static std::vector<TestCase> const tests = {
	    {"builtin palette is complete", test_builtin_palette_is_complete},
	    {"system mode defaults to light without a GUI application", test_system_mode_defaults_to_light_without_a_gui_application},
	    {"mode switch notifies and resolves palette", test_mode_switch_notifies_and_resolves_palette},
	    {"loads mode-specific and shared colors", test_loads_mode_specific_and_shared_colors},
	    {"unknown and invalid entries fall back per key", test_unknown_and_invalid_entries_fall_back_per_key},
	    {"malformed palette keeps previous theme", test_malformed_palette_keeps_previous_theme},
	    {"missing file reports error", test_missing_file_reports_error},
	    {"file URL is accepted", test_file_url_is_accepted},
	    {"reset restores builtin palette", test_reset_restores_builtin_palette},
	};
	return tests;
}

} // namespace

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	std::cout << std::unitbuf;

	std::size_t failedTests = 0;
	for (auto const &test : all_tests()) {
		int const failuresBefore = failures;
		std::cout << "[ RUN      ] " << test.name << "\n";
		test.run();
		if (failures == failuresBefore) {
			std::cout << "[       OK ] " << test.name << "\n";
		}
		else {
			++failedTests;
			std::cout << "[  FAILED  ] " << test.name << "\n";
		}
	}

	std::size_t const passed = all_tests().size() - failedTests;
	std::cout << (failedTests == 0 ? "PASSED" : "FAILED") << ": " << passed << "/" << all_tests().size() << " tests\n";
	return failedTests == 0 ? 0 : 1;
}
