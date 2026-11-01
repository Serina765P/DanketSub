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

/// @file theme_manager.h
/// @brief Light / dark / follow-system palette resolution for the Qt client

#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace danket::ui {

/// Owns the semantic colour palette rendered by the QML workspace.
///
/// The manager resolves three modes (system / light / dark) against built-in
/// colours and an optional palette loaded from an external JSON file. Every
/// change emits paletteChanged(), so QML bindings refresh without a restart.
class ThemeManager : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged FINAL)
	Q_PROPERTY(bool isDark READ isDark NOTIFY paletteChanged FINAL)
	Q_PROPERTY(QVariantMap palette READ palette NOTIFY paletteChanged FINAL)
	Q_PROPERTY(QString paletteName READ paletteName NOTIFY paletteChanged FINAL)
	Q_PROPERTY(QString palettePath READ palettePath NOTIFY paletteChanged FINAL)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
	enum class Mode {
		System,
		Light,
		Dark,
	};
	Q_ENUM(Mode)

	explicit ThemeManager(QObject *parent = nullptr);
	~ThemeManager() override;

	Mode mode() const { return mode_; }
	void setMode(Mode mode);

	bool isDark() const;
	QVariantMap palette() const;
	QString paletteName() const { return custom_.name; }
	QString palettePath() const { return custom_.path; }
	QString lastError() const { return lastError_; }

	/// Resolve a semantic key ("windowBg", "waveformBg", …).
	/// Unknown keys return an invalid QColor.
	Q_INVOKABLE QColor color(QString const &key) const;

	/// Replace the custom palette with the JSON file at @p path.
	/// Accepts plain paths and file:// URLs. A missing or malformed file leaves
	/// the current palette untouched and reports through lastError().
	Q_INVOKABLE bool loadPalette(QString const &path);

	/// Return to the built-in colours for the active mode.
	Q_INVOKABLE void resetPalette();

	/// Semantic keys understood by palettes, in documentation order.
	static QStringList paletteKeys();

signals:
	void modeChanged();
	void paletteChanged();
	void lastErrorChanged();

private:
	struct CustomPalette {
		QString path;
		QString name;
		QVariantMap shared;
		QVariantMap light;
		QVariantMap dark;

		bool isEmpty() const { return path.isEmpty() && shared.isEmpty() && light.isEmpty() && dark.isEmpty(); }
		void clear() { *this = CustomPalette{}; }
	};

	void detectSystemScheme();
	void setLastError(QString const &error);
	QVariantMap resolvePalette() const;

	Mode mode_ = Mode::System;
	bool systemDark_ = false;
	CustomPalette custom_;
	QString lastError_;
};

} // namespace danket::ui
