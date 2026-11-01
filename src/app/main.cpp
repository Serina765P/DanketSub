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

/// @file main.cpp
/// @brief DanketSub Qt Quick client entry point

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QtQml/qqmlextensionplugin.h>

#include "ribbonui.h"
#include "ribbontheme.h"

#ifdef RIBBONUI_BUILD_STATIC_LIB
Q_IMPORT_QML_PLUGIN(RibbonUIPlugin)
Q_IMPORT_QML_PLUGIN(DanketSubPlugin)
#endif

int main(int argc, char *argv[]) {
	// RibbonUI must configure the Qt Quick window before the application object
	// exists (alpha buffer, HiDPI rounding, Basic controls style).
	RibbonUI::init();

#if defined(Q_OS_WIN)
	// RibbonUI defaults to OpenGL; Direct3D 11 gives the DWM blur pipeline
	// proper hardware composition on Windows 11.
	qputenv("QSG_RHI_BACKEND", "d3d11");
	qputenv("QT_QPA_DISABLE_REDIRECTION_SURFACE", "1");
#endif

	QGuiApplication app(argc, argv);
	QGuiApplication::setApplicationName(QStringLiteral("DanketSub"));
	QGuiApplication::setApplicationDisplayName(QStringLiteral("DanketSub"));
	QGuiApplication::setApplicationVersion(QStringLiteral(DANKETSUB_VERSION));
	QGuiApplication::setOrganizationName(QStringLiteral("DanketSub"));

	// Fluent-style ribbon: transparent title bar over the DWM material.
	RibbonTheme::getInstance()->modernStyle(true);

	QCommandLineParser parser;
	parser.setApplicationDescription(
	    QCoreApplication::translate("main", "Modern subtitle timing and typesetting client"));
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption const paletteOption(
	    {QStringLiteral("p"), QStringLiteral("palette")},
	    QCoreApplication::translate("main", "Load the theme palette from <file> at startup."),
	    QCoreApplication::translate("main", "file"));
	parser.addOption(paletteOption);
	parser.process(app);

	QQmlApplicationEngine engine;
	RibbonUI::registerTypes(nullptr);
	engine.rootContext()->setContextProperty(QStringLiteral("startupPalette"),
	                                         parser.value(paletteOption));

	QObject::connect(
	    &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
	    [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

	engine.loadFromModule("DanketSub", "Main");

	if (engine.rootObjects().isEmpty()) {
		qCritical("Failed to load the DanketSub QML module");
		return EXIT_FAILURE;
	}

	return app.exec();
}
