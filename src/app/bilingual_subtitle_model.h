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

/// @file bilingual_subtitle_model.h
/// @brief Qt table model over the core bilingual row binder

#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace danket {
class BilingualBinder;
class CoreContext;
}

namespace danket::ui {

/// Editable side-by-side bilingual subtitle grid.
///
/// The model owns the ASS document and the core BilingualBinder that pairs
/// source and target dialogue lines. One logical table row maps to a pair
/// (or to a single unpaired line); the two text and style columns expose the
/// physical lines, while timing columns edit both halves of a pair at once.
class BilingualSubtitleModel : public QAbstractTableModel {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(int pairCount READ pairCount NOTIFY rowsChanged FINAL)
	Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged FINAL)
	Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged FINAL)
	Q_PROPERTY(QString fileName READ fileName NOTIFY filePathChanged FINAL)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
	Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged FINAL)

public:
	enum Column {
		ColIndex = 0,
		ColBindStatus,
		ColStartTime,
		ColEndTime,
		ColDuration,
		ColSourceText,
		ColTargetText,
		ColSourceStyle,
		ColTargetStyle,
		ColSyncId,
		ColumnCount,
	};
	Q_ENUM(Column)

	enum Role {
		IndexRole = Qt::UserRole + 1,
		IsLinkedRole,
		StartRole,
		EndRole,
		DurationRole,
		SourceTextRole,
		TargetTextRole,
		SourceStyleRole,
		TargetStyleRole,
		SyncIdRole,
		HasSourceRole,
		HasTargetRole,
	};
	Q_ENUM(Role)

	explicit BilingualSubtitleModel(QObject *parent = nullptr);
	~BilingualSubtitleModel() override;

	int pairCount() const { return rowCount(); }
	bool modified() const { return modified_; }
	QString filePath() const { return filePath_; }
	QString fileName() const;
	QString lastError() const { return lastError_; }
	int currentRow() const { return currentRow_; }
	void setCurrentRow(int row);

	int rowCount(QModelIndex const &parent = QModelIndex()) const override;
	int columnCount(QModelIndex const &parent = QModelIndex()) const override;
	QVariant data(QModelIndex const &index, int role = Qt::DisplayRole) const override;
	bool setData(QModelIndex const &index, QVariant const &value, int role = Qt::EditRole) override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(QModelIndex const &index) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE bool loadFile(QUrl const &url);
	Q_INVOKABLE bool saveFile(QUrl const &url);
	Q_INVOKABLE bool save();
	Q_INVOKABLE void newDocument();
	Q_INVOKABLE bool loadAssText(QString const &text);
	Q_INVOKABLE QString exportAssText() const;

	Q_INVOKABLE int appendPair();
	Q_INVOKABLE bool setSourceText(int row, QString const &text);
	Q_INVOKABLE bool setTargetText(int row, QString const &text);
	Q_INVOKABLE bool setStartTime(int row, QString const &timecode);
	Q_INVOKABLE bool setEndTime(int row, QString const &timecode);
	Q_INVOKABLE bool setRowTime(int row, QString const &start, QString const &end);
	Q_INVOKABLE bool deleteSide(int row, bool source);
	Q_INVOKABLE bool deleteRow(int row);
	Q_INVOKABLE bool unbindRow(int row);

signals:
	void rowsChanged();
	void modifiedChanged();
	void filePathChanged();
	void lastErrorChanged();
	void currentRowChanged();
	void rowEdited(int row);

private:
	void loadDefaultDocument();
	void resetRows();
	void setFilePath(QString const &path);
	void setModified(bool modified);
	void setLastError(QString const &error);
	bool applyText(int row, bool source, QString const &text);
	bool applyTimes(int row, QString const &startText, QString const &endText);

	std::unique_ptr<danket::CoreContext> context_;
	std::unique_ptr<danket::BilingualBinder> binder_;
	QString filePath_;
	QString lastError_;
	bool modified_ = false;
	int currentRow_ = -1;
};

} // namespace danket::ui
