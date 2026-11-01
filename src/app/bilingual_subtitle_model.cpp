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

/// @file bilingual_subtitle_model.cpp
/// @brief ASS document ownership, bilingual row binding and edit routing

// AssFile uses Aegisub's own Signal, whose member is named `slots`; Qt's
// keyword macro must not be in scope while that header is parsed.
#include "ass_dialogue.h"
#include "ass_file.h"

#include "bilingual_subtitle_model.h"

#include "danket/bilingual_binder.h"
#include "danket/core_context.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/exception.h>

#include <QFileInfo>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace danket::ui {

namespace {

std::string_view const kDocumentTemplate = R"([Script Info]
Title: Untitled
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080
WrapStyle: 0
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1
Style: Default_CN,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
)";

BilingualRow const *findRow(BilingualBinder const &binder, int row) {
	auto const &rows = binder.Rows();
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return nullptr;
	return &rows[static_cast<std::size_t>(row)];
}

int rowIndexForLine(BilingualBinder const &binder, AssDialogue const *line) {
	auto const &rows = binder.Rows();
	for (std::size_t index = 0; index < rows.size(); ++index) {
		if (rows[index].source == line || rows[index].target == line)
			return static_cast<int>(index);
	}
	return -1;
}

QString dialogueText(AssDialogue const *line) {
	return line ? QString::fromStdString(std::string(line->GetText())) : QString();
}

QString dialogueStyle(AssDialogue const *line) {
	return line ? QString::fromStdString(line->Style.get()) : QString();
}

double durationSeconds(BilingualRow const &row) {
	return static_cast<double>(static_cast<int>(row.End()) - static_cast<int>(row.Start())) / 1000.0;
}

/// Parse an ASS timecode, rejecting anything that is not a time-like string.
/// Accepts an optional leading/trailing whitespace, requires at least one `:`
/// and only digits plus `.`/`,`, then defers to agi::Time for the value.
std::optional<agi::Time> parseTimecode(QString const &text) {
	QString const trimmed = text.trimmed();
	if (trimmed.isEmpty())
		return std::nullopt;

	std::string const value = trimmed.toStdString();
	bool has_colon = false;
	for (char const c : value) {
		if (c == ':') {
			has_colon = true;
			continue;
		}
		if (c == '.' || c == ',')
			continue;
		if (c < '0' || c > '9')
			return std::nullopt;
	}

	if (!has_colon)
		return std::nullopt;
	return agi::Time(value);
}

/// Resolve a QUrl that may be a file:// URL or a plain path.
QString localPath(QUrl const &url) {
	return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

BilingualSubtitleModel::BilingualSubtitleModel(QObject *parent)
: QAbstractTableModel(parent)
, context_(std::make_unique<danket::CoreContext>())
, binder_(std::make_unique<danket::BilingualBinder>(context_->Subs()))
{
	loadDefaultDocument();
	currentRow_ = rowCount() > 0 ? 0 : -1;
}

BilingualSubtitleModel::~BilingualSubtitleModel() = default;

void BilingualSubtitleModel::loadDefaultDocument() {
	context_->LoadFromText(kDocumentTemplate);
	binder_->Refresh();
	(void)binder_->AddPair(agi::Time(0), agi::Time(2000),
		std::string_view{}, std::string_view{}, "Default", "Default_CN");
}

void BilingualSubtitleModel::resetRows() {
	beginResetModel();
	endResetModel();
	emit rowsChanged();
	setCurrentRow(currentRow_);
}

QString BilingualSubtitleModel::fileName() const {
	if (filePath_.isEmpty())
		return tr("Untitled");
	return QFileInfo(filePath_).fileName();
}

void BilingualSubtitleModel::setFilePath(QString const &path) {
	if (filePath_ == path)
		return;
	filePath_ = path;
	emit filePathChanged();
}

void BilingualSubtitleModel::setModified(bool modified) {
	if (modified_ == modified)
		return;
	modified_ = modified;
	emit modifiedChanged();
}

void BilingualSubtitleModel::setLastError(QString const &error) {
	if (lastError_ == error)
		return;
	lastError_ = error;
	emit lastErrorChanged();
}

void BilingualSubtitleModel::setCurrentRow(int row) {
	int const count = rowCount();
	int const clamped = count <= 0 ? -1 : std::clamp(row, 0, count - 1);
	if (clamped == currentRow_)
		return;
	currentRow_ = clamped;
	emit currentRowChanged();
}

int BilingualSubtitleModel::rowCount(QModelIndex const &parent) const {
	if (parent.isValid())
		return 0;
	return static_cast<int>(binder_->Rows().size());
}

int BilingualSubtitleModel::columnCount(QModelIndex const &parent) const {
	return parent.isValid() ? 0 : ColumnCount;
}

QHash<int, QByteArray> BilingualSubtitleModel::roleNames() const {
	static QHash<int, QByteArray> const roles = {
		{Qt::DisplayRole, QByteArrayLiteral("display")},
		{Qt::DecorationRole, QByteArrayLiteral("decoration")},
		{Qt::EditRole, QByteArrayLiteral("edit")},
		{Qt::ToolTipRole, QByteArrayLiteral("toolTip")},
		{IndexRole, QByteArrayLiteral("rowIndex")},
		{IsLinkedRole, QByteArrayLiteral("isLinked")},
		{StartRole, QByteArrayLiteral("startTimeMs")},
		{EndRole, QByteArrayLiteral("endTimeMs")},
		{DurationRole, QByteArrayLiteral("durationSec")},
		{SourceTextRole, QByteArrayLiteral("sourceText")},
		{TargetTextRole, QByteArrayLiteral("targetText")},
		{SourceStyleRole, QByteArrayLiteral("sourceStyle")},
		{TargetStyleRole, QByteArrayLiteral("targetStyle")},
		{SyncIdRole, QByteArrayLiteral("syncId")},
		{HasSourceRole, QByteArrayLiteral("hasSource")},
		{HasTargetRole, QByteArrayLiteral("hasTarget")},
	};
	return roles;
}

QVariant BilingualSubtitleModel::data(QModelIndex const &index, int role) const {
	if (!index.isValid() || index.column() < 0 || index.column() >= ColumnCount)
		return {};

	BilingualRow const *row = findRow(*binder_, index.row());
	if (!row)
		return {};

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case ColIndex:
			return index.row() + 1;
		case ColBindStatus:
			return row->linked ? tr("Linked") : tr("Single");
		case ColStartTime:
			return QString::fromStdString(row->Start().GetAssFormatted());
		case ColEndTime:
			return QString::fromStdString(row->End().GetAssFormatted());
		case ColDuration:
			return QString::number(durationSeconds(*row), 'f', 2) + QStringLiteral(" s");
		case ColSourceText:
			return dialogueText(row->source);
		case ColTargetText:
			return dialogueText(row->target);
		case ColSourceStyle:
			return dialogueStyle(row->source);
		case ColTargetStyle:
			return dialogueStyle(row->target);
		case ColSyncId:
			return QString::fromStdString(row->sync_id);
		default:
			return {};
		}
	}

	switch (role) {
	case IndexRole:
		return index.row() + 1;
	case IsLinkedRole:
		return row->linked;
	case StartRole:
		return static_cast<qint64>(static_cast<int>(row->Start()));
	case EndRole:
		return static_cast<qint64>(static_cast<int>(row->End()));
	case DurationRole:
		return durationSeconds(*row);
	case SourceTextRole:
		return dialogueText(row->source);
	case TargetTextRole:
		return dialogueText(row->target);
	case SourceStyleRole:
		return dialogueStyle(row->source);
	case TargetStyleRole:
		return dialogueStyle(row->target);
	case SyncIdRole:
		return QString::fromStdString(row->sync_id);
	case HasSourceRole:
		return row->source != nullptr;
	case HasTargetRole:
		return row->target != nullptr;
	default:
		return {};
	}
}

bool BilingualSubtitleModel::setData(QModelIndex const &index, QVariant const &value, int role) {
	if (!index.isValid() || role != Qt::EditRole)
		return false;

	QString const text = value.toString();
	switch (index.column()) {
	case ColStartTime:
		return setStartTime(index.row(), text);
	case ColEndTime:
		return setEndTime(index.row(), text);
	case ColSourceText:
		return setSourceText(index.row(), text);
	case ColTargetText:
		return setTargetText(index.row(), text);
	default:
		return false;
	}
}

QVariant BilingualSubtitleModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);

	switch (section) {
	case ColIndex:
		return tr("#");
	case ColBindStatus:
		return tr("Binding");
	case ColStartTime:
		return tr("Start");
	case ColEndTime:
		return tr("End");
	case ColDuration:
		return tr("Duration");
	case ColSourceText:
		return tr("Source");
	case ColTargetText:
		return tr("Target");
	case ColSourceStyle:
		return tr("Source style");
	case ColTargetStyle:
		return tr("Target style");
	case ColSyncId:
		return tr("Sync ID");
	default:
		return {};
	}
}

Qt::ItemFlags BilingualSubtitleModel::flags(QModelIndex const &index) const {
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags result = QAbstractTableModel::flags(index);
	switch (index.column()) {
	case ColStartTime:
	case ColEndTime:
	case ColSourceText:
	case ColTargetText:
		result |= Qt::ItemIsEditable;
		break;
	default:
		break;
	}
	return result;
}

bool BilingualSubtitleModel::loadFile(QUrl const &url) {
	QString const path = localPath(url);
	if (path.isEmpty()) {
		setLastError(tr("No file path given"));
		return false;
	}

	try {
		context_->LoadFile(std::filesystem::path(path.toStdWString()));
	}
	catch (agi::Exception const &error) {
		setLastError(QString::fromStdString(error.GetMessage()));
		return false;
	}
	catch (std::exception const &error) {
		setLastError(QString::fromUtf8(error.what()));
		return false;
	}
	catch (...) {
		setLastError(tr("Cannot load the subtitle file"));
		return false;
	}

	binder_->Refresh();
	resetRows();
	setCurrentRow(0);
	setFilePath(path);
	setModified(false);
	setLastError({});
	return true;
}

bool BilingualSubtitleModel::saveFile(QUrl const &url) {
	QString const path = localPath(url);
	if (path.isEmpty()) {
		setLastError(tr("No file path given"));
		return false;
	}

	try {
		context_->SaveFile(std::filesystem::path(path.toStdWString()));
	}
	catch (agi::Exception const &error) {
		setLastError(QString::fromStdString(error.GetMessage()));
		return false;
	}
	catch (std::exception const &error) {
		setLastError(QString::fromUtf8(error.what()));
		return false;
	}
	catch (...) {
		setLastError(tr("Cannot save the subtitle file"));
		return false;
	}

	setFilePath(path);
	setModified(false);
	setLastError({});
	return true;
}

bool BilingualSubtitleModel::save() {
	if (filePath_.isEmpty()) {
		setLastError(tr("No file path to save to"));
		return false;
	}
	return saveFile(QUrl::fromLocalFile(filePath_));
}

void BilingualSubtitleModel::newDocument() {
	loadDefaultDocument();
	resetRows();
	setFilePath({});
	setModified(false);
	setLastError({});
	setCurrentRow(0);
}

bool BilingualSubtitleModel::loadAssText(QString const &text) {
	try {
		context_->LoadFromText(text.toStdString());
	}
	catch (agi::Exception const &error) {
		setLastError(QString::fromStdString(error.GetMessage()));
		return false;
	}
	catch (std::exception const &error) {
		setLastError(QString::fromUtf8(error.what()));
		return false;
	}
	catch (...) {
		setLastError(tr("Cannot parse the subtitle text"));
		return false;
	}

	binder_->Refresh();
	resetRows();
	setCurrentRow(0);
	setFilePath({});
	setModified(false);
	setLastError({});
	return true;
}

QString BilingualSubtitleModel::exportAssText() const {
	return QString::fromStdString(context_->SaveToText());
}

int BilingualSubtitleModel::appendPair() {
	agi::Time start(0);
	agi::Time end(2000);
	auto const &rows = binder_->Rows();
	if (!rows.empty()) {
		start = agi::Time(static_cast<int>(rows.back().End()) + 200);
		end = agi::Time(static_cast<int>(start) + 2000);
	}

	QString const target_style = context_->Subs().GetStyle("Default_CN")
		? QStringLiteral("Default_CN")
		: QStringLiteral("Default");

	int const added = binder_->AddPair(start, end, {}, {}, "Default", target_style.toStdString());
	if (added < 0)
		return -1;

	resetRows();
	setModified(true);
	setCurrentRow(added);
	return added;
}

bool BilingualSubtitleModel::setSourceText(int row, QString const &text) {
	return applyText(row, true, text);
}

bool BilingualSubtitleModel::setTargetText(int row, QString const &text) {
	return applyText(row, false, text);
}

bool BilingualSubtitleModel::setStartTime(int row, QString const &timecode) {
	BilingualRow const *line = findRow(*binder_, row);
	if (!line)
		return false;

	QString const endText = QString::fromStdString(line->End().GetAssFormatted(true));
	return applyTimes(row, timecode, endText);
}

bool BilingualSubtitleModel::setEndTime(int row, QString const &timecode) {
	BilingualRow const *line = findRow(*binder_, row);
	if (!line)
		return false;

	QString const startText = QString::fromStdString(line->Start().GetAssFormatted(true));
	return applyTimes(row, startText, timecode);
}

bool BilingualSubtitleModel::setRowTime(int row, QString const &start, QString const &end) {
	return applyTimes(row, start, end);
}

bool BilingualSubtitleModel::deleteSide(int row, bool source) {
	if (!findRow(*binder_, row))
		return false;

	BilingualSide const side = source ? BilingualSide::Source : BilingualSide::Target;
	if (!binder_->DeleteSide(row, side))
		return false;

	resetRows();
	setModified(true);
	return true;
}

bool BilingualSubtitleModel::deleteRow(int row) {
	if (!findRow(*binder_, row))
		return false;
	if (!binder_->DeleteRow(row))
		return false;

	resetRows();
	setModified(true);
	return true;
}

bool BilingualSubtitleModel::unbindRow(int row) {
	if (!findRow(*binder_, row))
		return false;
	if (!binder_->Unbind(row))
		return false;

	resetRows();
	setModified(true);
	return true;
}

bool BilingualSubtitleModel::applyText(int row, bool source, QString const &text) {
	BilingualRow const *line = findRow(*binder_, row);
	if (!line)
		return false;

	BilingualSide const side = source ? BilingualSide::Source : BilingualSide::Target;
	QString const current = dialogueText(line->Line(side));
	if (current == text)
		return true;

	// Clearing one half of a pair removes that physical line and can move the
	// logical row when the pair was not stored adjacently, so the row layout
	// (not just its contents) is invalidated.
	bool const restructures = line->linked && text.isEmpty();
	AssDialogue *survivor = restructures
		? line->Line(source ? BilingualSide::Target : BilingualSide::Source)
		: nullptr;

	if (!binder_->SetText(row, side, text.toStdString()))
		return false;

	setModified(true);
	int edited_row = row;
	if (restructures) {
		resetRows();
		int const moved = rowIndexForLine(*binder_, survivor);
		if (moved >= 0) {
			edited_row = moved;
			setCurrentRow(moved);
		}
	}
	else {
		emit dataChanged(index(row, ColIndex), index(row, ColumnCount - 1));
	}
	emit rowEdited(edited_row);
	return true;
}

bool BilingualSubtitleModel::applyTimes(int row, QString const &startText, QString const &endText) {
	auto const start = parseTimecode(startText);
	if (!start)
		return false;
	auto const end = parseTimecode(endText);
	if (!end)
		return false;

	BilingualRow const *line = findRow(*binder_, row);
	if (!line)
		return false;
	if (static_cast<int>(*end) < static_cast<int>(*start))
		return false;

	if (static_cast<int>(line->Start()) == static_cast<int>(*start) &&
	    static_cast<int>(line->End()) == static_cast<int>(*end))
		return true;

	if (!binder_->SetTime(row, *start, *end))
		return false;

	setModified(true);
	emit dataChanged(index(row, ColStartTime), index(row, ColDuration));
	emit rowEdited(row);
	return true;
}

} // namespace danket::ui
