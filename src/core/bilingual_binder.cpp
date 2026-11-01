// Copyright (c) 2026, DanketSub contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "danket/bilingual_binder.h"

#include "ass_dialogue.h"
#include "ass_file.h"

#include <cctype>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace danket {
namespace {

bool IsSpace(char c) {
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view Trim(std::string_view text) {
	while (!text.empty() && IsSpace(text.front()))
		text.remove_prefix(1);
	while (!text.empty() && IsSpace(text.back()))
		text.remove_suffix(1);
	return text;
}

bool IsValidSyncId(std::string_view id) {
	if (id.empty())
		return false;

	for (char c : id) {
		if (IsSpace(c) || c == ',' || c == ';' || c == '\r' || c == '\n')
			return false;
	}
	return true;
}

BilingualSide OppositeSide(BilingualSide side) {
	return side == BilingualSide::Source ? BilingualSide::Target : BilingualSide::Source;
}

std::string_view SideValue(BilingualSide side) {
	return side == BilingualSide::Source ? "src" : "dst";
}

void SetEffect(AssDialogue &line, std::string_view effect) {
	line.Effect = std::string(effect);
}

void SetTag(AssDialogue &line, std::string_view id, BilingualSide side) {
	SetEffect(line, BilingualBinder::FormatSyncTag(id, side));
}

/// Remove and dispose a single physical line; returns true if it was present
bool RemoveLine(AssFile &file, AssDialogue *line) {
	if (!line)
		return false;

	std::size_t removed = 0;
	file.Events.remove_and_dispose_if(
		[line, &removed](AssDialogue const &candidate) {
			if (&candidate != line)
				return false;
			++removed;
			return true;
		},
		[](AssDialogue *disposed) { delete disposed; });
	return removed == 1;
}

} // namespace
} // namespace danket

namespace danket {

bool BilingualRow::Has(BilingualSide side) const {
	return Line(side) != nullptr;
}

AssDialogue *BilingualRow::Line(BilingualSide side) const {
	return side == BilingualSide::Source ? source : target;
}

agi::Time BilingualRow::Start() const {
	if (source)
		return source->Start;
	if (target)
		return target->Start;
	return agi::Time(0);
}

agi::Time BilingualRow::End() const {
	if (source)
		return source->End;
	if (target)
		return target->End;
	return agi::Time(0);
}

BilingualBinder::BilingualBinder(AssFile &file)
: file(file)
{
	Refresh();
}

std::optional<SyncTag> BilingualBinder::ParseSyncTag(std::string_view effect) {
	std::string id;
	std::optional<BilingualSide> side;
	bool has_id = false;

	effect = Trim(effect);
	std::size_t pos = 0;
	while (pos <= effect.size()) {
		std::size_t end = effect.find(';', pos);
		std::string_view const token = Trim(effect.substr(pos,
			end == std::string_view::npos ? std::string_view::npos : end - pos));

		if (!token.empty()) {
			if (token.starts_with("sync-id:")) {
				if (has_id)
					return std::nullopt;

				std::string_view const value = token.substr(8);
				if (!IsValidSyncId(value))
					return std::nullopt;

				id.assign(value);
				has_id = true;
			}
			else if (token.starts_with("lang:")) {
				if (side)
					return std::nullopt;

				std::string_view const value = token.substr(5);
				if (value == "src")
					side = BilingualSide::Source;
				else if (value == "dst")
					side = BilingualSide::Target;
				else
					return std::nullopt;
			}
		}

		if (end == std::string_view::npos)
			break;
		pos = end + 1;
	}

	if (!has_id || !side)
		return std::nullopt;
	return SyncTag{std::move(id), *side};
}

std::string BilingualBinder::FormatSyncTag(std::string_view id, BilingualSide side) {
	std::string tag = "sync-id:";
	tag += id;
	tag += ";lang:";
	tag += SideValue(side);
	return tag;
}

std::string BilingualBinder::GenerateSyncId(AssFile const &file) {
	std::set<std::string> used;
	for (auto const &line : file.Events) {
		if (auto tag = ParseSyncTag(line.Effect.get()))
			used.insert(tag->id);
	}

	for (std::size_t index = 1;; ++index) {
		std::ostringstream candidate;
		candidate << "row-" << std::setw(2) << std::setfill('0') << index;
		if (!used.contains(candidate.str()))
			return candidate.str();
	}
}

void BilingualBinder::Refresh() {
	rows.clear();

	std::unordered_map<std::string, std::size_t> by_id;
	for (auto &line : file.Events) {
		auto const tag = ParseSyncTag(line.Effect.get());
		if (!tag) {
			// Untagged lines are single, source-only rows
			BilingualRow row;
			row.source = &line;
			rows.push_back(std::move(row));
			continue;
		}

		auto const found = by_id.find(tag->id);
		if (found != by_id.end() && !rows[found->second].Has(tag->side)) {
			BilingualRow &row = rows[found->second];
			if (tag->side == BilingualSide::Source)
				row.source = &line;
			else
				row.target = &line;
			row.linked = row.source && row.target;
			continue;
		}

		if (found == by_id.end()) {
			BilingualRow row;
			row.sync_id = tag->id;
			if (tag->side == BilingualSide::Source)
				row.source = &line;
			else
				row.target = &line;
			by_id.emplace(tag->id, rows.size());
			rows.push_back(std::move(row));
			continue;
		}

		// A second line for an already-occupied side cannot join the pair; it
		// stays a single, unbound row on the side named by its own tag
		BilingualRow row;
		if (tag->side == BilingualSide::Source)
			row.source = &line;
		else
			row.target = &line;
		rows.push_back(std::move(row));
	}

	// The source side is canonical for timing
	for (auto &row : rows) {
		if (row.source && row.target) {
			row.target->Start = row.source->Start;
			row.target->End = row.source->End;
		}
	}
}

bool BilingualBinder::SetTime(int row, agi::Time start, agi::Time end) {
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return false;
	if (end < start)
		return false;

	BilingualRow &target = rows[static_cast<std::size_t>(row)];
	bool const source_matches = !target.source || (target.source->Start == start && target.source->End == end);
	bool const target_matches = !target.target || (target.target->Start == start && target.target->End == end);
	if (source_matches && target_matches)
		return false;

	if (target.source) {
		target.source->Start = start;
		target.source->End = end;
	}
	if (target.target) {
		target.target->Start = start;
		target.target->End = end;
	}

	AssDialogue *single_line = nullptr;
	if (!(target.source && target.target))
		single_line = target.source ? target.source : target.target;

	file.Commit("edit bilingual timing", AssFile::COMMIT_DIAG_TIME, -1, single_line);
	return true;
}

bool BilingualBinder::SetText(int row, BilingualSide side, std::string_view text) {
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return false;

	BilingualRow &target = rows[static_cast<std::size_t>(row)];
	BilingualSide const other_side = OppositeSide(side);
	AssDialogue *const existing = target.Line(side);
	AssDialogue *const counterpart = target.Line(other_side);

	if (existing) {
		if (!text.empty()) {
			existing->SetText(text);
			file.Commit("edit bilingual text", AssFile::COMMIT_DIAG_TEXT, -1, existing);
			return true;
		}

		if (counterpart) {
			// Clearing one half of a pair removes only that line; the other half
			// survives as an ordinary untagged line rather than cascading.
			SetEffect(*counterpart, std::string_view());
			RemoveLine(file, existing);
			file.Commit("delete bilingual line", AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_META, -1);
			Refresh();
			return true;
		}

		existing->SetText(std::string_view());
		bool const had_stale_tag = ParseSyncTag(existing->Effect.get()).has_value();
		if (had_stale_tag) {
			SetEffect(*existing, std::string_view());
			target.sync_id.clear();
		}

		int const type = AssFile::COMMIT_DIAG_TEXT | (had_stale_tag ? AssFile::COMMIT_DIAG_META : 0);
		file.Commit("edit bilingual text", type, -1, existing);
		return true;
	}

	if (text.empty() || !counterpart)
		return false;

	std::string sync_id = target.sync_id;
	if (sync_id.empty())
		sync_id = GenerateSyncId(file);

	auto *created = new AssDialogue;
	created->Start = counterpart->Start;
	created->End = counterpart->End;
	created->Style = counterpart->Style.get().empty() ? std::string("Default") : counterpart->Style.get();
	created->SetText(text);
	SetTag(*created, sync_id, side);
	SetTag(*counterpart, sync_id, other_side);

	if (side == BilingualSide::Source)
		file.Events.insert(file.iterator_to(*counterpart), *created);
	else
		file.Events.insert(++file.iterator_to(*counterpart), *created);

	file.Commit("bind bilingual pair", AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL, -1);
	Refresh();
	return true;
}

bool BilingualBinder::DeleteSide(int row, BilingualSide side) {
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return false;

	BilingualRow &target = rows[static_cast<std::size_t>(row)];
	AssDialogue *const line = target.Line(side);
	if (!line)
		return false;

	AssDialogue *const counterpart = target.Line(OppositeSide(side));
	if (counterpart)
		SetEffect(*counterpart, std::string_view());

	RemoveLine(file, line);
	int const type = AssFile::COMMIT_DIAG_ADDREM | (counterpart ? AssFile::COMMIT_DIAG_META : 0);
	file.Commit("delete bilingual line", type, -1);
	Refresh();
	return true;
}

bool BilingualBinder::DeleteRow(int row) {
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return false;

	BilingualRow &target = rows[static_cast<std::size_t>(row)];
	if (!target.source && !target.target)
		return false;

	RemoveLine(file, target.source);
	RemoveLine(file, target.target);
	file.Commit("delete bilingual pair", AssFile::COMMIT_DIAG_ADDREM, -1);
	Refresh();
	return true;
}

bool BilingualBinder::Unbind(int row) {
	if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
		return false;

	BilingualRow &target = rows[static_cast<std::size_t>(row)];
	if (!target.source && !target.target)
		return false;

	if (target.source)
		SetEffect(*target.source, std::string_view());
	if (target.target)
		SetEffect(*target.target, std::string_view());

	file.Commit("unbind bilingual pair", AssFile::COMMIT_DIAG_META, -1);
	Refresh();
	return true;
}

int BilingualBinder::AddPair(agi::Time start, agi::Time end,
	std::string_view source_text,
	std::string_view target_text,
	std::string_view source_style,
	std::string_view target_style)
{
	if (end < start)
		return -1;

	std::string const sync_id = GenerateSyncId(file);

	auto *source = new AssDialogue;
	source->Start = start;
	source->End = end;
	source->Style = std::string(source_style);
	source->SetText(source_text);
	SetTag(*source, sync_id, BilingualSide::Source);

	auto *target = new AssDialogue;
	target->Start = start;
	target->End = end;
	target->Style = std::string(target_style);
	target->SetText(target_text);
	SetTag(*target, sync_id, BilingualSide::Target);

	file.Events.push_back(*source);
	file.Events.push_back(*target);

	file.Commit("bind bilingual pair", AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL, -1);
	Refresh();

	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (rows[i].sync_id == sync_id)
			return static_cast<int>(i);
	}
	return -1;
}

}
