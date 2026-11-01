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

/// @file theme_manager.cpp
/// @brief Built-in palettes, JSON palette loading and system theme tracking

#include "theme_manager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStyleHints>
#include <QUrl>

namespace danket::ui {

namespace {

QVariantMap makePalette(bool dark) {
	QVariantMap palette;
	auto set = [&](char const *key, char const *value) {
		palette.insert(QString::fromLatin1(key), QColor(QString::fromLatin1(value)));
	};

	if (dark) {
		set("windowBg", "#202124");
		set("surfaceBg", "#2b2b2e");
		set("surfaceAltBg", "#323236");
		set("borderColor", "#3f3f44");
		set("textPrimary", "#f2f2f2");
		set("textSecondary", "#a9acb1");
		set("accent", "#4cc2ff");
		set("selectionBg", "#274b6b");
		set("gridZebraBg", "#2a2b2f");
		set("waveformBg", "#191a1d");
		set("waveformSelection", "#f2b84b");
		set("cursorColor", "#4cc2ff");
		set("danger", "#ff99a4");
	}
	else {
		set("windowBg", "#f3f3f3");
		set("surfaceBg", "#ffffff");
		set("surfaceAltBg", "#f7f8fa");
		set("borderColor", "#d6d8db");
		set("textPrimary", "#1c1c1e");
		set("textSecondary", "#5f6368");
		set("accent", "#0f6cbd");
		set("selectionBg", "#cce4ff");
		set("gridZebraBg", "#f7f8fa");
		set("waveformBg", "#e9edf2");
		set("waveformSelection", "#e8a93a");
		set("cursorColor", "#0f6cbd");
		set("danger", "#c42b1c");
	}
	return palette;
}

/// Copy the recognised colour entries of @p object into @p target.
/// Unknown keys and unparsable colours are ignored: the built-in value for
/// that key stays in place, so a partial or forward-compatible palette still
/// produces a usable theme.
void mergeColors(QJsonObject const &object, QVariantMap &target) {
	static QStringList const known = ThemeManager::paletteKeys();
	for (auto it = object.begin(); it != object.end(); ++it) {
		if (!known.contains(it.key()) || !it.value().isString())
			continue;
		QColor const color = QColor::fromString(it.value().toString());
		if (color.isValid())
			target.insert(it.key(), color);
	}
}

} // namespace

ThemeManager::ThemeManager(QObject *parent)
	: QObject(parent) {
	auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
	if (gui && gui->styleHints()) {
		systemDark_ = gui->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
		connect(gui->styleHints(), &QStyleHints::colorSchemeChanged, this,
		        [this](Qt::ColorScheme) { detectSystemScheme(); });
	}
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::detectSystemScheme() {
	auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
	if (!gui || !gui->styleHints())
		return;

	bool const dark = gui->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
	if (dark == systemDark_)
		return;

	systemDark_ = dark;
	if (mode_ == Mode::System)
		emit paletteChanged();
}

void ThemeManager::setMode(Mode mode) {
	if (mode_ == mode)
		return;

	mode_ = mode;
	emit modeChanged();
	emit paletteChanged();
}

bool ThemeManager::isDark() const {
	return mode_ == Mode::Dark || (mode_ == Mode::System && systemDark_);
}

QVariantMap ThemeManager::palette() const {
	return resolvePalette();
}

QVariantMap ThemeManager::resolvePalette() const {
	QVariantMap palette = makePalette(isDark());
	for (auto it = custom_.shared.begin(); it != custom_.shared.end(); ++it)
		palette.insert(it.key(), it.value());
	QVariantMap const &modePalette = isDark() ? custom_.dark : custom_.light;
	for (auto it = modePalette.begin(); it != modePalette.end(); ++it)
		palette.insert(it.key(), it.value());
	return palette;
}

QColor ThemeManager::color(QString const &key) const {
	return resolvePalette().value(key).value<QColor>();
}

QStringList ThemeManager::paletteKeys() {
	static QStringList const keys = {
		QStringLiteral("windowBg"),
		QStringLiteral("surfaceBg"),
		QStringLiteral("surfaceAltBg"),
		QStringLiteral("borderColor"),
		QStringLiteral("textPrimary"),
		QStringLiteral("textSecondary"),
		QStringLiteral("accent"),
		QStringLiteral("selectionBg"),
		QStringLiteral("gridZebraBg"),
		QStringLiteral("waveformBg"),
		QStringLiteral("waveformSelection"),
		QStringLiteral("cursorColor"),
		QStringLiteral("danger"),
	};
	return keys;
}

bool ThemeManager::loadPalette(QString const &path) {
	QString localPath = path;
	if (QUrl const url(path); url.isLocalFile())
		localPath = url.toLocalFile();

	QFile file(localPath);
	if (!file.open(QIODevice::ReadOnly)) {
		setLastError(tr("Cannot open palette file: %1").arg(localPath));
		return false;
	}

	QJsonParseError parseError{};
	QJsonDocument const document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		setLastError(tr("Palette JSON is invalid at offset %1: %2")
		                 .arg(parseError.offset)
		                 .arg(parseError.errorString()));
		return false;
	}
	if (!document.isObject()) {
		setLastError(tr("Palette JSON must contain a top-level object"));
		return false;
	}

	QJsonObject const root = document.object();
	QJsonObject shared;
	QJsonObject light;
	QJsonObject dark;

	auto readSection = [&](QString const &name, QJsonObject &target) {
		if (!root.contains(name))
			return true;
		QJsonValue const value = root.value(name);
		if (!value.isObject()) {
			setLastError(tr("Palette section \"%1\" must be an object").arg(name));
			return false;
		}
		target = value.toObject();
		return true;
	};

	if (!readSection(QStringLiteral("light"), light) || !readSection(QStringLiteral("dark"), dark))
		return false;

	QVariantMap sharedColors;
	QVariantMap lightColors;
	QVariantMap darkColors;
	mergeColors(root, sharedColors);
	mergeColors(light, lightColors);
	mergeColors(dark, darkColors);

	custom_.shared = std::move(sharedColors);
	custom_.light = std::move(lightColors);
	custom_.dark = std::move(darkColors);
	custom_.path = QFileInfo(localPath).absoluteFilePath();
	custom_.name = root.value(QStringLiteral("name")).toString();
	if (custom_.name.isEmpty())
		custom_.name = QFileInfo(localPath).completeBaseName();
	setLastError({});
	emit paletteChanged();
	return true;
}

void ThemeManager::resetPalette() {
	if (custom_.isEmpty()) {
		setLastError({});
		return;
	}

	custom_.clear();
	setLastError({});
	emit paletteChanged();
}

void ThemeManager::setLastError(QString const &error) {
	if (lastError_ == error)
		return;

	lastError_ = error;
	emit lastErrorChanged();
}

} // namespace danket::ui
