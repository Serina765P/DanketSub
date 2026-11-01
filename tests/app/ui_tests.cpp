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

/// @file ui_tests.cpp
/// @brief Interactive smoke tests for the window shell and theme hot-swap

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QVariantMap>

#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>

#include "ribbonui.h"
#include "test_support.h"
#include "theme_manager.h"

Q_IMPORT_QML_PLUGIN(RibbonUIPlugin)
Q_IMPORT_QML_PLUGIN(DanketSubPlugin)

using namespace danketsub_test;

namespace {

#define CHECK(label, condition) check_true(label, (condition), __FILE__, __LINE__)
#define CHECK_COLOR_AT(label, actual, expected) check_color(label, (actual), (expected), __FILE__, __LINE__)

QQuickItem *find_item(QQuickItem *root, QString const &name) {
	if (!root)
		return nullptr;
	if (root->objectName() == name)
		return root;
	for (QQuickItem *child : root->childItems()) {
		if (QQuickItem *found = find_item(child, name))
			return found;
	}
	return nullptr;
}

} // namespace

int main(int argc, char *argv[]) {
	RibbonUI::init();
	QGuiApplication app(argc, argv);

	QQmlApplicationEngine engine;
	RibbonUI::registerTypes(nullptr);
	engine.loadFromModule("DanketSub", "Main");
	if (engine.rootObjects().isEmpty()) {
		std::fprintf(stderr, "FAIL: Main.qml did not load\n");
		return 1;
	}

	auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
	if (!window) {
		std::fprintf(stderr, "FAIL: root object is not a QQuickWindow\n");
		return 1;
	}

	window->show();
	static_cast<void>(QTest::qWaitForWindowExposed(window));
	QTest::qWait(700);

	auto *ribbon = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("ribbonTabBar")));
	if (!ribbon) {
		std::fprintf(stderr, "FAIL: ribbon tab bar not found\n");
		return 1;
	}
	auto *workspace = window->findChild<QObject *>(QStringLiteral("workspaceRoot"));
	if (!workspace) {
		std::fprintf(stderr, "FAIL: workspace not found\n");
		return 1;
	}

	auto *theme = engine.singletonInstance<danket::ui::ThemeManager *>("DanketSub", "ThemeManager");
	if (!theme) {
		std::fprintf(stderr, "FAIL: ThemeManager singleton not found\n");
		return 1;
	}

	auto *background = find_item(window->contentItem(), QStringLiteral("workspaceBackground"));
	auto *statusBar = find_item(window->contentItem(), QStringLiteral("workspaceStatusBar"));
	CHECK("workspace background exists", background != nullptr);
	CHECK("workspace status bar exists", statusBar != nullptr);
	if (background)
		CHECK_COLOR_AT("workspace background uses windowBg",
		            background->property("color").value<QColor>(), QColor("#f3f3f3"));
	if (statusBar)
		CHECK_COLOR_AT("status bar uses surfaceAltBg",
		            statusBar->property("color").value<QColor>(), QColor("#f7f8fa"));

	// --- Ribbon folding -----------------------------------------------------
	// The tab-bar double-click gesture is RibbonUI's own fold path and is
	// exercised against a real window; here we cover both explicit toggles.
	CHECK("ribbon starts expanded", !ribbon->property("folded").toBool());

	auto *compactButton = find_item(window->contentItem(), QStringLiteral("compactRibbonButton"));
	CHECK("compact ribbon button exists", compactButton != nullptr);

	QTest::keyClick(window, Qt::Key_F1, Qt::ControlModifier);
	QTest::qWait(400);
	CHECK("Ctrl+F1 folds the ribbon", ribbon->property("folded").toBool());

	if (compactButton) {
		QMetaObject::invokeMethod(compactButton, "clicked");
		QTest::qWait(400);
		CHECK("the Compact button expands the ribbon", !ribbon->property("folded").toBool());
	}

	// --- Theme hot switching ------------------------------------------------
	QVariantMap colors = workspace->property("colors").toMap();
	QColor const lightWindow = colors.value(QStringLiteral("windowBg")).value<QColor>();

	theme->setMode(danket::ui::ThemeManager::Mode::Dark);
	QTest::qWait(50);
	CHECK("dark mode resolves to dark", theme->isDark());
	colors = workspace->property("colors").toMap();
	CHECK_COLOR_AT("workspace repaints from the dark palette",
	            colors.value(QStringLiteral("windowBg")).value<QColor>(),
	            QColor(QStringLiteral("#202124")));
	if (background)
		CHECK_COLOR_AT("workspace background repaints dark",
		               background->property("color").value<QColor>(), QColor("#202124"));
	if (statusBar)
		CHECK_COLOR_AT("status bar repaints dark",
		               statusBar->property("color").value<QColor>(), QColor("#323236"));
	CHECK("dark palette differs from light", colors.value(QStringLiteral("windowBg")).value<QColor>() != lightWindow);

	theme->setMode(danket::ui::ThemeManager::Mode::Light);
	QTest::qWait(50);
	colors = workspace->property("colors").toMap();
	CHECK_COLOR_AT("workspace returns to the light palette",
	            colors.value(QStringLiteral("windowBg")).value<QColor>(),
	            QColor(QStringLiteral("#f3f3f3")));

	// --- External palette + fallback ---------------------------------------
	QTemporaryDir dir;
	CHECK("temporary palette directory", dir.isValid());
	QString const goodPath = write_palette(dir, QStringLiteral("good.json"), R"({
		"name": "UI Test",
		"light": { "windowBg": "#123456", "waveformBg": "#654321" }
	})");
	QString const brokenPath = write_palette(dir, QStringLiteral("broken.json"), R"({ "light": { "windowBg": )");

	CHECK("external palette loads", theme->loadPalette(goodPath));
	QTest::qWait(50);
	colors = workspace->property("colors").toMap();
	CHECK_COLOR_AT("custom window background applied", colors.value(QStringLiteral("windowBg")).value<QColor>(), QColor("#123456"));
	CHECK_COLOR_AT("custom waveform background applied", colors.value(QStringLiteral("waveformBg")).value<QColor>(), QColor("#654321"));
	if (background)
		CHECK_COLOR_AT("workspace background follows the file palette",
		               background->property("color").value<QColor>(), QColor("#123456"));

	CHECK("broken palette is rejected", !theme->loadPalette(brokenPath));
	QTest::qWait(50);
	CHECK("broken palette reports an error", !theme->lastError().isEmpty());
	colors = workspace->property("colors").toMap();
	CHECK_COLOR_AT("failed load keeps the previous palette", colors.value(QStringLiteral("windowBg")).value<QColor>(), QColor("#123456"));

	theme->resetPalette();
	QTest::qWait(50);
	colors = workspace->property("colors").toMap();
	CHECK_COLOR_AT("reset restores the built-in palette", colors.value(QStringLiteral("windowBg")).value<QColor>(), QColor("#f3f3f3"));
	if (background)
		CHECK_COLOR_AT("reset restores the workspace background",
		               background->property("color").value<QColor>(), QColor("#f3f3f3"));

	CHECK("theme test failures", failures == 0);
	std::printf(failures == 0 ? "PASSED\n" : "FAILED: %d\n", failures);
	return failures == 0 ? 0 : 1;
}
