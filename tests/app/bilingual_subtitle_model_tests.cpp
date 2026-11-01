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

/// @file bilingual_subtitle_model_tests.cpp
/// @brief Self-contained tests for the bilingual grid table model

#include "bilingual_subtitle_model.h"
#include "test_support.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QFile>
#include <QModelIndex>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>

#include <cmath>
#include <iostream>
#include <vector>

using namespace danketsub_test;
using danket::ui::BilingualSubtitleModel;

namespace {

struct Recorder {
	int rowsChanged = 0;
	int modifiedChanged = 0;
	int filePathChanged = 0;
	int lastErrorChanged = 0;
	int currentRowChanged = 0;
	int rowEdited = 0;
	int modelResets = 0;
	int dataChanges = 0;
	int lastEditedRow = -1;

	explicit Recorder(BilingualSubtitleModel &model) {
		QObject::connect(&model, &BilingualSubtitleModel::rowsChanged, &model, [this] { ++rowsChanged; });
		QObject::connect(&model, &BilingualSubtitleModel::modifiedChanged, &model, [this] { ++modifiedChanged; });
		QObject::connect(&model, &BilingualSubtitleModel::filePathChanged, &model, [this] { ++filePathChanged; });
		QObject::connect(&model, &BilingualSubtitleModel::lastErrorChanged, &model, [this] { ++lastErrorChanged; });
		QObject::connect(&model, &BilingualSubtitleModel::currentRowChanged, &model, [this] { ++currentRowChanged; });
		QObject::connect(&model, &BilingualSubtitleModel::rowEdited, &model, [this](int row) {
			++rowEdited;
			lastEditedRow = row;
		});
		QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [this] { ++modelResets; });
		QObject::connect(&model, &QAbstractItemModel::dataChanged, &model,
			[this](QModelIndex const &, QModelIndex const &, QList<int> const &) { ++dataChanges; });
	}
};

bool almostEqual(double actual, double expected) {
	return std::fabs(actual - expected) < 1e-6;
}

int countOccurrences(QString const &text, QString const &needle) {
	int count = 0;
	int position = 0;
	while ((position = text.indexOf(needle, position)) >= 0) {
		++count;
		position += needle.size();
	}
	return count;
}

QStringList dialogueLines(QString const &text) {
	QStringList result;
	for (QString line : text.split(QLatin1Char('\n'))) {
		if (line.endsWith(QLatin1Char('\r')))
			line.chop(1);
		if (line.startsWith(QStringLiteral("Dialogue:")))
			result.append(line);
	}
	return result;
}

QStringList sectionHeaders(QString const &text) {
	QStringList result;
	for (QString line : text.split(QLatin1Char('\n'))) {
		line = line.trimmed();
		if (line.startsWith(QLatin1Char('[')))
			result.append(line);
	}
	return result;
}

QString fixtureTwoPairsAndSingleLine() {
	static char const *const text =
		"[Script Info]\n"
		"Title: Bilingual Fixture\n"
		"ScriptType: v4.00+\n"
		"PlayResX: 1920\n"
		"PlayResY: 1080\n"
		"\n"
		"[V4+ Styles]\n"
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
		"Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"Style: Default_CN,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"\n"
		"[Events]\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,sync-id:row-01;lang:src,Hello source\n"
		"Dialogue: 0,0:00:01.00,0:00:03.50,Default_CN,,0,0,0,sync-id:row-01;lang:dst,Hello target\n"
		"Dialogue: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,sync-id:row-02;lang:src,Second source\n"
		"Dialogue: 0,0:00:05.00,0:00:06.00,Default_CN,,0,0,0,sync-id:row-02;lang:dst,Second target\n"
		"Dialogue: 0,0:00:07.00,0:00:08.00,Default,,0,0,0,,Loose line\n";
	return QString::fromLatin1(text);
}

QString fixtureTargetOnlyLine() {
	static char const *const text =
		"[Script Info]\n"
		"Title: Target Only\n"
		"ScriptType: v4.00+\n"
		"PlayResX: 1920\n"
		"PlayResY: 1080\n"
		"\n"
		"[V4+ Styles]\n"
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
		"Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"Style: Default_CN,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"\n"
		"[Events]\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Dialogue: 0,0:00:02.00,0:00:04.00,Default_CN,,0,0,0,sync-id:row-01;lang:dst,Orphan target\n";
	return QString::fromLatin1(text);
}

QString fixtureInterleavedPair() {
	static char const *const text =
		"[Script Info]\n"
		"Title: Interleaved Pair\n"
		"ScriptType: v4.00+\n"
		"PlayResX: 1920\n"
		"PlayResY: 1080\n"
		"\n"
		"[V4+ Styles]\n"
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
		"Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"\n"
		"[Events]\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,sync-id:row-01;lang:src,First source\n"
		"Dialogue: 0,0:00:02.00,0:00:03.00,Default,,0,0,0,sync-id:row-02;lang:src,Interleaved source\n"
		"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,sync-id:row-01;lang:dst,First target\n";
	return QString::fromLatin1(text);
}

void test_clearing_interleaved_pair_keeps_survivor_selected() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.loadAssText(fixtureInterleavedPair()));
	CHECK_EQ_INT(model.rowCount(), 2);
	CHECK_STRING(model.index(0, 5).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("First source"));
	CHECK_STRING(model.index(0, 6).data(BilingualSubtitleModel::TargetTextRole).toString(),
		QStringLiteral("First target"));
	CHECK_STRING(model.index(1, 5).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Interleaved source"));

	Recorder recorder(model);
	CHECK_TRUE(model.setSourceText(0, QString()));

	// The surviving target line moved after the interleaved row; the model
	// resets and moves the selection onto it instead of leaving the cursor on
	// the unrelated row that now occupies index 0.
	CHECK_EQ_INT(model.rowCount(), 2);
	CHECK_STRING(model.index(0, 5).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Interleaved source"));
	CHECK_TRUE(!model.index(1, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(!model.index(1, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_STRING(model.index(1, 5).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("First target"));
	CHECK_EQ_INT(model.currentRow(), 1);
	CHECK_EQ_INT(recorder.modelResets, 1);
	CHECK_EQ_INT(recorder.currentRowChanged, 1);
}

void test_constructor_starts_with_one_linked_pair() {
	BilingualSubtitleModel model;

	CHECK_EQ_INT(model.pairCount(), 1);
	CHECK_EQ_INT(model.rowCount(), 1);
	CHECK_EQ_INT(model.columnCount(), BilingualSubtitleModel::ColumnCount);
	CHECK_TRUE(!model.modified());
	CHECK_TRUE(model.filePath().isEmpty());
	CHECK_STRING(model.fileName(), QStringLiteral("Untitled"));
	CHECK_TRUE(model.lastError().isEmpty());
	CHECK_EQ_INT(model.currentRow(), 0);

	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 0);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 2000);
	CHECK_TRUE(almostEqual(model.index(0, 0).data(BilingualSubtitleModel::DurationRole).toDouble(), 2.0));
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SourceStyleRole).toString(), QStringLiteral("Default"));
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::TargetStyleRole).toString(), QStringLiteral("Default_CN"));
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(), QString());
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(), QString());

	QString const syncId = model.index(0, 0).data(BilingualSubtitleModel::SyncIdRole).toString();
	CHECK_TRUE(!syncId.isEmpty());

	CHECK_STRING(model.headerData(0, Qt::Horizontal).toString(), QStringLiteral("#"));
	CHECK_STRING(model.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Binding"));
	CHECK_STRING(model.headerData(2, Qt::Horizontal).toString(), QStringLiteral("Start"));
	CHECK_STRING(model.headerData(3, Qt::Horizontal).toString(), QStringLiteral("End"));
	CHECK_STRING(model.headerData(4, Qt::Horizontal).toString(), QStringLiteral("Duration"));
	CHECK_STRING(model.headerData(5, Qt::Horizontal).toString(), QStringLiteral("Source"));
	CHECK_STRING(model.headerData(6, Qt::Horizontal).toString(), QStringLiteral("Target"));
	CHECK_STRING(model.headerData(7, Qt::Horizontal).toString(), QStringLiteral("Source style"));
	CHECK_STRING(model.headerData(8, Qt::Horizontal).toString(), QStringLiteral("Target style"));
	CHECK_STRING(model.headerData(9, Qt::Horizontal).toString(), QStringLiteral("Sync ID"));

	QHash<int, QByteArray> const roles = model.roleNames();
	CHECK_EQ_INT(roles.size(), 16);
	QStringList names;
	for (auto const &name : roles)
		names.append(QString::fromLatin1(name));
	names.sort();
	CHECK_TRUE(names == QStringList({QStringLiteral("decoration"), QStringLiteral("display"),
		QStringLiteral("durationSec"), QStringLiteral("edit"), QStringLiteral("endTimeMs"),
		QStringLiteral("hasSource"), QStringLiteral("hasTarget"), QStringLiteral("isLinked"),
		QStringLiteral("rowIndex"), QStringLiteral("sourceStyle"), QStringLiteral("sourceText"),
		QStringLiteral("startTimeMs"), QStringLiteral("syncId"), QStringLiteral("targetStyle"),
		QStringLiteral("targetText"), QStringLiteral("toolTip")}));
}

void test_roles_and_display_expose_pairs() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.loadAssText(fixtureTwoPairsAndSingleLine()));
	CHECK_EQ_INT(model.rowCount(), 3);
	CHECK_EQ_INT(model.pairCount(), 3);

	QModelIndex const first = model.index(0, 0);
	CHECK_EQ_INT(first.data(BilingualSubtitleModel::IndexRole).toInt(), 1);
	CHECK_TRUE(first.data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(first.data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(first.data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_EQ_INT(first.data(BilingualSubtitleModel::StartRole).toInt(), 1000);
	CHECK_EQ_INT(first.data(BilingualSubtitleModel::EndRole).toInt(), 3500);
	CHECK_TRUE(almostEqual(first.data(BilingualSubtitleModel::DurationRole).toDouble(), 2.5));
	CHECK_STRING(first.data(BilingualSubtitleModel::SourceTextRole).toString(), QStringLiteral("Hello source"));
	CHECK_STRING(first.data(BilingualSubtitleModel::TargetTextRole).toString(), QStringLiteral("Hello target"));
	CHECK_STRING(first.data(BilingualSubtitleModel::SourceStyleRole).toString(), QStringLiteral("Default"));
	CHECK_STRING(first.data(BilingualSubtitleModel::TargetStyleRole).toString(), QStringLiteral("Default_CN"));
	CHECK_STRING(first.data(BilingualSubtitleModel::SyncIdRole).toString(), QStringLiteral("row-01"));

	QModelIndex const single = model.index(2, 0);
	CHECK_TRUE(!single.data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(single.data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(!single.data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_EQ_INT(single.data(BilingualSubtitleModel::StartRole).toInt(), 7000);
	CHECK_EQ_INT(single.data(BilingualSubtitleModel::EndRole).toInt(), 8000);
	CHECK_TRUE(almostEqual(single.data(BilingualSubtitleModel::DurationRole).toDouble(), 1.0));
	CHECK_STRING(single.data(BilingualSubtitleModel::SourceTextRole).toString(), QStringLiteral("Loose line"));
	CHECK_STRING(single.data(BilingualSubtitleModel::TargetTextRole).toString(), QString());
	CHECK_STRING(single.data(BilingualSubtitleModel::SyncIdRole).toString(), QString());

	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColIndex).data().toString(), QStringLiteral("1"));
	CHECK_STRING(model.index(2, BilingualSubtitleModel::ColIndex).data().toString(), QStringLiteral("3"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColBindStatus).data().toString(), QStringLiteral("Linked"));
	CHECK_STRING(model.index(2, BilingualSubtitleModel::ColBindStatus).data().toString(), QStringLiteral("Single"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColStartTime).data().toString(), QStringLiteral("0:00:01.00"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColEndTime).data().toString(), QStringLiteral("0:00:03.50"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColDuration).data().toString(), QStringLiteral("2.50 s"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColSourceText).data().toString(), QStringLiteral("Hello source"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColTargetText).data().toString(), QStringLiteral("Hello target"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColSourceStyle).data().toString(), QStringLiteral("Default"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColTargetStyle).data().toString(), QStringLiteral("Default_CN"));
	CHECK_STRING(model.index(0, BilingualSubtitleModel::ColSyncId).data().toString(), QStringLiteral("row-01"));

	CHECK_TRUE(model.flags(model.index(0, BilingualSubtitleModel::ColStartTime)).testFlag(Qt::ItemIsEditable));
	CHECK_TRUE(model.flags(model.index(0, BilingualSubtitleModel::ColEndTime)).testFlag(Qt::ItemIsEditable));
	CHECK_TRUE(model.flags(model.index(0, BilingualSubtitleModel::ColSourceText)).testFlag(Qt::ItemIsEditable));
	CHECK_TRUE(model.flags(model.index(0, BilingualSubtitleModel::ColTargetText)).testFlag(Qt::ItemIsEditable));
	CHECK_TRUE(!model.flags(model.index(0, BilingualSubtitleModel::ColDuration)).testFlag(Qt::ItemIsEditable));
	CHECK_TRUE(!model.flags(model.index(0, BilingualSubtitleModel::ColSyncId)).testFlag(Qt::ItemIsEditable));

	CHECK_TRUE(model.setData(model.index(0, BilingualSubtitleModel::ColSourceText),
		QStringLiteral("Edited source"), Qt::EditRole));
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Edited source"));
	CHECK_TRUE(model.exportAssText().contains(QStringLiteral("Edited source")));
	CHECK_TRUE(!model.setData(model.index(0, BilingualSubtitleModel::ColDuration),
		QStringLiteral("1.00"), Qt::EditRole));
}

void test_time_edits_synchronize_both_lines() {
	BilingualSubtitleModel model;
	Recorder recorder(model);

	CHECK_TRUE(model.setRowTime(0, QStringLiteral("0:00:00.00"), QStringLiteral("0:00:02.00")));
	CHECK_TRUE(!model.modified());
	CHECK_EQ_INT(recorder.modifiedChanged, 0);
	CHECK_EQ_INT(recorder.rowEdited, 0);
	CHECK_EQ_INT(recorder.dataChanges, 0);
	CHECK_EQ_INT(recorder.rowsChanged, 0);
	CHECK_EQ_INT(recorder.modelResets, 0);

	CHECK_TRUE(model.setStartTime(0, QStringLiteral(" 0:00:01.50 ")));
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 1500);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 2000);
	CHECK_TRUE(model.modified());
	CHECK_EQ_INT(recorder.modifiedChanged, 1);
	CHECK_EQ_INT(recorder.rowEdited, 1);
	CHECK_EQ_INT(recorder.dataChanges, 1);
	CHECK_EQ_INT(recorder.rowsChanged, 0);
	CHECK_EQ_INT(recorder.modelResets, 0);
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("0:00:01.50")), 2);
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("sync-id:")), 2);

	CHECK_TRUE(model.setEndTime(0, QStringLiteral("0:00:04.25")));
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 4250);
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("0:00:01.50")), 2);
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("0:00:04.25")), 2);

	QString const before = model.exportAssText();
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("abc")));
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("")));
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("not a time")));
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("0:00:01.00x")));
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("1500")));
	CHECK_TRUE(!model.setStartTime(0, QStringLiteral("0:00:09.00")));
	CHECK_TRUE(!model.setEndTime(0, QStringLiteral("0:00:01.00")));
	CHECK_TRUE(!model.setRowTime(0, QStringLiteral("0:00:05.00"), QStringLiteral("0:00:04.00")));
	CHECK_TRUE(!model.setRowTime(0, QStringLiteral("0:00:01.00"), QStringLiteral("bad")));
	CHECK_TRUE(!model.setStartTime(-1, QStringLiteral("0:00:00.00")));
	CHECK_TRUE(!model.setEndTime(7, QStringLiteral("0:00:02.00")));
	CHECK_STRING(model.exportAssText(), before);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 1500);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 4250);

	CHECK_TRUE(model.setData(model.index(0, BilingualSubtitleModel::ColStartTime),
		QStringLiteral("0:00:02.50"), Qt::EditRole));
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 2500);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 4250);
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("0:00:02.50")), 2);
	CHECK_EQ_INT(recorder.rowsChanged, 0);
	CHECK_EQ_INT(recorder.modelResets, 0);
}

void test_deleting_one_side_keeps_the_counterpart() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.loadAssText(fixtureTwoPairsAndSingleLine()));
	CHECK_EQ_INT(model.rowCount(), 3);
	CHECK_TRUE(!model.modified());

	Recorder recorder(model);
	CHECK_TRUE(model.deleteSide(0, false));
	CHECK_EQ_INT(model.pairCount(), 3);
	CHECK_EQ_INT(model.rowCount(), 3);
	CHECK_TRUE(!model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(!model.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Hello source"));
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(), QString());
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 1000);
	CHECK_EQ_INT(model.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 3500);
	CHECK_EQ_INT(model.currentRow(), 0);
	CHECK_EQ_INT(recorder.rowsChanged, 1);
	CHECK_EQ_INT(recorder.modelResets, 1);
	CHECK_EQ_INT(recorder.modifiedChanged, 1);
	CHECK_TRUE(model.modified());

	QString exportText = model.exportAssText();
	CHECK_EQ_INT(dialogueLines(exportText).size(), 4);
	CHECK_EQ_INT(countOccurrences(exportText, QStringLiteral("sync-id:")), 2);
	CHECK_TRUE(exportText.contains(QStringLiteral("Hello source")));
	CHECK_TRUE(!exportText.contains(QStringLiteral("Hello target")));

	BilingualSubtitleModel cleared;
	CHECK_TRUE(cleared.loadAssText(fixtureTwoPairsAndSingleLine()));
	Recorder clearRecorder(cleared);
	CHECK_TRUE(cleared.setTargetText(0, QString()));
	CHECK_EQ_INT(cleared.pairCount(), 3);
	CHECK_TRUE(!cleared.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(!cleared.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_STRING(cleared.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Hello source"));
	CHECK_EQ_INT(clearRecorder.modifiedChanged, 1);
	CHECK_EQ_INT(clearRecorder.dataChanges, 0);
	CHECK_EQ_INT(clearRecorder.rowEdited, 1);
	CHECK_EQ_INT(clearRecorder.rowsChanged, 1);
	CHECK_EQ_INT(clearRecorder.modelResets, 1);
	CHECK_TRUE(cleared.modified());

	exportText = cleared.exportAssText();
	CHECK_EQ_INT(dialogueLines(exportText).size(), 4);
	CHECK_EQ_INT(countOccurrences(exportText, QStringLiteral("sync-id:")), 2);
	CHECK_TRUE(exportText.contains(QStringLiteral("Hello source")));
	CHECK_TRUE(!exportText.contains(QStringLiteral("Hello target")));
}

void test_empty_side_is_filled_by_text_edits() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.deleteSide(0, false));
	CHECK_TRUE(!model.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());

	CHECK_TRUE(model.setTargetText(0, QStringLiteral("Filled target")));
	CHECK_EQ_INT(model.rowCount(), 1);
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(),
		QStringLiteral("Filled target"));
	CHECK_TRUE(!model.index(0, 0).data(BilingualSubtitleModel::SyncIdRole).toString().isEmpty());

	QString exportText = model.exportAssText();
	CHECK_EQ_INT(countOccurrences(exportText, QStringLiteral("sync-id:")), 2);
	CHECK_EQ_INT(countOccurrences(exportText, QStringLiteral("lang:src")), 1);
	CHECK_EQ_INT(countOccurrences(exportText, QStringLiteral("lang:dst")), 1);
	CHECK_TRUE(exportText.contains(QStringLiteral("Filled target")));

	Recorder recorder(model);
	CHECK_TRUE(model.setTargetText(0, QStringLiteral("Filled target")));
	CHECK_TRUE(model.setSourceText(0, QString()));
	CHECK_EQ_INT(recorder.modifiedChanged, 0);
	CHECK_EQ_INT(recorder.dataChanges, 0);
	CHECK_EQ_INT(recorder.rowEdited, 0);
	CHECK_EQ_INT(recorder.rowsChanged, 0);
	CHECK_EQ_INT(recorder.modelResets, 0);
	CHECK_TRUE(model.modified());

	BilingualSubtitleModel other;
	CHECK_TRUE(other.loadAssText(fixtureTargetOnlyLine()));
	CHECK_EQ_INT(other.rowCount(), 1);
	CHECK_TRUE(!other.index(0, 0).data(BilingualSubtitleModel::HasSourceRole).toBool());
	CHECK_TRUE(other.index(0, 0).data(BilingualSubtitleModel::HasTargetRole).toBool());
	CHECK_STRING(other.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(),
		QStringLiteral("Orphan target"));
	CHECK_TRUE(other.setSourceText(0, QStringLiteral("Filled source")));
	CHECK_TRUE(other.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_EQ_INT(other.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 2000);
	CHECK_EQ_INT(other.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 4000);
	CHECK_EQ_INT(countOccurrences(other.exportAssText(), QStringLiteral("sync-id:")), 2);
	CHECK_TRUE(other.exportAssText().contains(QStringLiteral("Filled source")));
	CHECK_TRUE(other.exportAssText().contains(QStringLiteral("Orphan target")));

	BilingualSubtitleModel untouched;
	Recorder untouchedRecorder(untouched);
	CHECK_TRUE(untouched.setSourceText(0, QString()));
	CHECK_TRUE(untouched.setTargetText(0, QString()));
	CHECK_TRUE(!untouched.modified());
	CHECK_EQ_INT(untouchedRecorder.modifiedChanged, 0);
	CHECK_EQ_INT(untouchedRecorder.dataChanges, 0);
}

void test_structural_edits_reset_the_model() {
	BilingualSubtitleModel model;
	Recorder recorder(model);

	int const appended = model.appendPair();
	CHECK_EQ_INT(appended, 1);
	CHECK_EQ_INT(model.pairCount(), 2);
	CHECK_EQ_INT(model.currentRow(), 1);
	CHECK_TRUE(model.modified());
	CHECK_EQ_INT(recorder.rowsChanged, 1);
	CHECK_EQ_INT(recorder.modelResets, 1);

	CHECK_TRUE(model.unbindRow(0));
	CHECK_EQ_INT(model.pairCount(), 3);
	CHECK_EQ_INT(recorder.rowsChanged, 2);
	CHECK_EQ_INT(recorder.modelResets, 2);
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SyncIdRole).toString(), QString());
	CHECK_TRUE(!model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("sync-id:")), 2);

	CHECK_TRUE(model.deleteRow(0));
	CHECK_EQ_INT(model.pairCount(), 2);
	CHECK_EQ_INT(recorder.rowsChanged, 3);
	CHECK_TRUE(model.deleteRow(0));
	CHECK_EQ_INT(model.pairCount(), 1);
	CHECK_TRUE(model.deleteRow(0));
	CHECK_EQ_INT(model.pairCount(), 0);
	CHECK_EQ_INT(model.rowCount(), 0);
	CHECK_EQ_INT(model.currentRow(), -1);
	CHECK_EQ_INT(dialogueLines(model.exportAssText()).size(), 0);

	CHECK_EQ_INT(model.appendPair(), 0);
	CHECK_EQ_INT(model.pairCount(), 1);
	CHECK_EQ_INT(model.currentRow(), 0);
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_EQ_INT(countOccurrences(model.exportAssText(), QStringLiteral("sync-id:")), 2);

	CHECK_TRUE(!model.deleteSide(4, true));
	CHECK_TRUE(!model.deleteRow(-1));
	CHECK_TRUE(!model.unbindRow(9));
	CHECK_TRUE(!model.setSourceText(9, QStringLiteral("nope")));
	CHECK_TRUE(!model.setTargetText(9, QStringLiteral("nope")));
}

void test_file_round_trip_preserves_pairing() {
	QTemporaryDir dir;
	CHECK_TRUE(dir.isValid());
	QString const path = dir.filePath(QStringLiteral("round-trip.ass"));
	QUrl const url = QUrl::fromLocalFile(path);

	BilingualSubtitleModel model;
	CHECK_TRUE(model.setSourceText(0, QStringLiteral("Saved source")));
	CHECK_TRUE(model.setTargetText(0, QStringLiteral("Saved target")));
	CHECK_TRUE(model.setRowTime(0, QStringLiteral("0:00:01.25"), QStringLiteral("0:00:03.75")));
	CHECK_TRUE(model.saveFile(url));
	CHECK_TRUE(!model.modified());
	CHECK_STRING(model.filePath(), url.toLocalFile());
	CHECK_STRING(model.fileName(), QStringLiteral("round-trip.ass"));
	CHECK_TRUE(model.lastError().isEmpty());

	QFile saved(path);
	CHECK_TRUE(saved.open(QIODevice::ReadOnly));
	QString const savedText = QString::fromUtf8(saved.readAll());
	saved.close();
	CHECK_EQ_INT(countOccurrences(savedText, QStringLiteral("sync-id:")), 2);
	CHECK_TRUE(savedText.contains(QStringLiteral("Saved source")));
	CHECK_TRUE(savedText.contains(QStringLiteral("Saved target")));
	CHECK_TRUE(savedText.contains(QStringLiteral("0:00:01.25")));

	BilingualSubtitleModel loaded;
	CHECK_TRUE(loaded.loadFile(url));
	CHECK_TRUE(!loaded.modified());
	CHECK_STRING(loaded.filePath(), url.toLocalFile());
	CHECK_TRUE(loaded.lastError().isEmpty());
	CHECK_EQ_INT(loaded.currentRow(), 0);
	CHECK_EQ_INT(loaded.pairCount(), model.pairCount());
	CHECK_TRUE(loaded.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_STRING(loaded.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Saved source"));
	CHECK_STRING(loaded.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(),
		QStringLiteral("Saved target"));
	CHECK_EQ_INT(loaded.index(0, 0).data(BilingualSubtitleModel::StartRole).toInt(), 1250);
	CHECK_EQ_INT(loaded.index(0, 0).data(BilingualSubtitleModel::EndRole).toInt(), 3750);

	CHECK_TRUE(loaded.setSourceText(0, QStringLiteral("Resaved source")));
	CHECK_TRUE(loaded.modified());
	CHECK_TRUE(loaded.save());
	CHECK_TRUE(!loaded.modified());
	CHECK_STRING(loaded.filePath(), url.toLocalFile());

	QFile resaved(path);
	CHECK_TRUE(resaved.open(QIODevice::ReadOnly));
	QString const resavedText = QString::fromUtf8(resaved.readAll());
	resaved.close();
	CHECK_TRUE(resavedText.contains(QStringLiteral("Resaved source")));

	QString const beforeMissingLoad = loaded.exportAssText();
	CHECK_TRUE(!loaded.loadFile(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.ass")))));
	CHECK_TRUE(!loaded.lastError().isEmpty());
	CHECK_STRING(loaded.exportAssText(), beforeMissingLoad);
	CHECK_STRING(loaded.filePath(), url.toLocalFile());
	CHECK_TRUE(!loaded.modified());

	// Plain paths, without a file:// scheme, are accepted as well.
	QString const plainPath = dir.filePath(QStringLiteral("plain-path.ass"));
	BilingualSubtitleModel plainModel;
	CHECK_TRUE(plainModel.setSourceText(0, QStringLiteral("Plain source")));
	CHECK_TRUE(plainModel.saveFile(QUrl(plainPath)));
	CHECK_TRUE(!plainModel.modified());

	BilingualSubtitleModel plainLoaded;
	CHECK_TRUE(plainLoaded.loadFile(QUrl(plainPath)));
	CHECK_TRUE(!plainLoaded.modified());
	CHECK_STRING(plainLoaded.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(),
		QStringLiteral("Plain source"));

	QString const plainFilePath = plainModel.filePath();
	CHECK_TRUE(plainModel.setSourceText(0, QStringLiteral("Unsaved edit")));
	CHECK_TRUE(!plainModel.saveFile(QUrl::fromLocalFile(dir.filePath(QStringLiteral("no-such-dir/failed.ass")))));
	CHECK_TRUE(plainModel.modified());
	CHECK_TRUE(!plainModel.lastError().isEmpty());
	CHECK_STRING(plainModel.filePath(), plainFilePath);

	BilingualSubtitleModel untitled;
	CHECK_TRUE(!untitled.save());
	CHECK_TRUE(!untitled.lastError().isEmpty());
}

void test_malformed_ass_keeps_the_document() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.setSourceText(0, QStringLiteral("Keep me")));
	CHECK_TRUE(model.setTargetText(0, QStringLiteral("And me")));
	QString const before = model.exportAssText();
	int const rowsBefore = model.rowCount();

	CHECK_TRUE(!model.loadAssText(QStringLiteral("[Script Info]\nScriptType: v9.99\n")));
	CHECK_TRUE(!model.lastError().isEmpty());
	CHECK_STRING(model.exportAssText(), before);
	CHECK_EQ_INT(model.rowCount(), rowsBefore);
	CHECK_TRUE(model.modified());
	CHECK_TRUE(model.filePath().isEmpty());

	CHECK_TRUE(!model.loadAssText(QStringLiteral(
		"[Script Info]\n"
		"ScriptType: v4.00+\n"
		"\n"
		"[Events]\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,not-a-number,0,0,,text\n")));
	CHECK_TRUE(!model.lastError().isEmpty());
	CHECK_STRING(model.exportAssText(), before);
	CHECK_TRUE(model.modified());

	CHECK_TRUE(model.loadAssText(fixtureTwoPairsAndSingleLine()));
	CHECK_TRUE(model.lastError().isEmpty());
	CHECK_TRUE(!model.modified());
	CHECK_EQ_INT(model.rowCount(), 3);
	CHECK_EQ_INT(model.currentRow(), 0);
	CHECK_TRUE(model.filePath().isEmpty());

	model.newDocument();
	CHECK_TRUE(!model.modified());
	CHECK_TRUE(model.lastError().isEmpty());
	CHECK_EQ_INT(model.pairCount(), 1);
	CHECK_EQ_INT(model.currentRow(), 0);
	CHECK_TRUE(model.index(0, 0).data(BilingualSubtitleModel::IsLinkedRole).toBool());
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::SourceTextRole).toString(), QString());
	CHECK_STRING(model.index(0, 0).data(BilingualSubtitleModel::TargetTextRole).toString(), QString());
	CHECK_TRUE(model.exportAssText().contains(QStringLiteral("Title: Untitled")));
}

void test_export_is_standard_ass_with_effect_tags() {
	BilingualSubtitleModel model;
	CHECK_TRUE(model.setSourceText(0, QStringLiteral("Source one")));
	CHECK_TRUE(model.setTargetText(0, QStringLiteral("Target one")));
	CHECK_EQ_INT(model.appendPair(), 1);
	CHECK_TRUE(model.setSourceText(1, QStringLiteral("Source two")));
	CHECK_TRUE(model.setTargetText(1, QStringLiteral("Target two")));

	QString const text = model.exportAssText();
	QStringList const dialogues = dialogueLines(text);
	CHECK_EQ_INT(dialogues.size(), 2 * model.pairCount());
	CHECK_EQ_INT(countOccurrences(text, QStringLiteral("sync-id:")), 4);
	CHECK_EQ_INT(countOccurrences(text, QStringLiteral("lang:src")), 2);
	CHECK_EQ_INT(countOccurrences(text, QStringLiteral("lang:dst")), 2);
	for (QString const &line : dialogues) {
		CHECK_EQ_INT(line.split(QLatin1Char(',')).size(), 10);
		CHECK_TRUE(line.contains(QStringLiteral("sync-id:")));
	}

	QStringList const sections = sectionHeaders(text);
	CHECK_TRUE(!sections.isEmpty());
	for (QString const &section : sections) {
		// The core ASS writer emits Aegisub's own metadata section; the binder
		// itself must not introduce any custom script section.
		bool const standard = section == QStringLiteral("[Script Info]")
			|| section == QStringLiteral("[V4+ Styles]")
			|| section == QStringLiteral("[Events]")
			|| section == QStringLiteral("[Aegisub Project Garbage]");
		CHECK_TRUE(standard);
		CHECK_TRUE(!section.contains(QStringLiteral("Bilingual"), Qt::CaseInsensitive));
	}
	CHECK_TRUE(!text.contains(QStringLiteral("[Aegisub Extradata]")));
	CHECK_TRUE(!text.contains(QStringLiteral("[Fonts]")));
	CHECK_TRUE(!text.contains(QStringLiteral("[Graphics]")));
}

struct TestCase {
	char const *name;
	void (*run)();
};

std::vector<TestCase> const &all_tests() {
	static std::vector<TestCase> const tests = {
		{"clearing interleaved pair keeps survivor selected", test_clearing_interleaved_pair_keeps_survivor_selected},
		{"constructor starts with one linked pair", test_constructor_starts_with_one_linked_pair},
		{"roles and display expose pairs", test_roles_and_display_expose_pairs},
		{"time edits synchronize both lines", test_time_edits_synchronize_both_lines},
		{"deleting one side keeps the counterpart", test_deleting_one_side_keeps_the_counterpart},
		{"empty side is filled by text edits", test_empty_side_is_filled_by_text_edits},
		{"structural edits reset the model", test_structural_edits_reset_the_model},
		{"file round trip preserves pairing", test_file_round_trip_preserves_pairing},
		{"malformed ASS keeps the document", test_malformed_ass_keeps_the_document},
		{"export is standard ASS with Effect tags", test_export_is_standard_ass_with_effect_tags},
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
