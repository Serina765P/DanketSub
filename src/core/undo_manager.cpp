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

#include "danket/undo_manager.h"

#include "ass_attachment.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_info.h"
#include "ass_style.h"

#include <libaegisub/signal.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct UndoSnapshot {
	std::string description;
	int commit_id = 0;

	std::vector<std::pair<std::string, std::string>> script_info;
	std::vector<AssStyle> styles;
	std::vector<AssDialogueBase> events;
	std::vector<AssAttachment> attachments;
	std::vector<ExtradataEntry> extradata;
	ProjectProperties properties;
	uint32_t next_extradata_id = 0;

	explicit UndoSnapshot(AssFile const &file, std::string_view desc, int commit_id)
	: description(desc)
	, commit_id(commit_id)
	, attachments(file.Attachments)
	, extradata(file.Extradata)
	, properties(file.Properties)
	, next_extradata_id(file.next_extradata_id)
	{
		script_info.reserve(file.Info.size());
		for (auto const &info : file.Info)
			script_info.emplace_back(info.Key(), info.Value());

		styles.reserve(file.Styles.size());
		styles.assign(file.Styles.begin(), file.Styles.end());

		events.reserve(file.Events.size());
		events.assign(file.Events.begin(), file.Events.end());
	}
};

}

namespace danket {

struct UndoManager::Impl {
	AssFile &file;
	agi::signal::Connection connection;

	std::list<UndoSnapshot> undo_stack;
	std::list<UndoSnapshot> redo_stack;

	int commit_id = 0;
	int next_commit_id = 1;
	int max_depth = 100;
	bool applying = false;

	explicit Impl(AssFile &file)
	: file(file)
	{
		connection = file.AddUndoManager(&Impl::OnCommit, this);
	}

	void Trim() {
		while (static_cast<int>(undo_stack.size()) > max_depth)
			undo_stack.pop_front();
	}

	void OnCommit(AssFileCommit commit) {
		// Commits made while applying a snapshot must not create new history
		if (applying)
			return;

		// Empty descriptions are only used for the baseline state
		if (commit.message.empty() && !undo_stack.empty())
			return;

		commit_id = next_commit_id++;

		// Allow coalescing only if it's the last change and nothing has been
		// redone since it
		if (!undo_stack.empty() && commit_id == *commit.commit_id + 1 && redo_stack.empty()) {
			// If only one line changed just modify it instead of copying the file
			if (commit.single_line && commit.single_line->Group() == AssEntryGroup::DIALOGUE) {
				for (auto &line : undo_stack.back().events) {
					if (line.Id == commit.single_line->Id) {
						line = *commit.single_line;
						*commit.commit_id = commit_id;
						return;
					}
				}
			}

			undo_stack.pop_back();
		}

		redo_stack.clear();
		undo_stack.emplace_back(file, commit.message, commit_id);
		Trim();

		*commit.commit_id = commit_id;
	}

	void Apply(UndoSnapshot const &snapshot) {
		// Keep the old lines alive until the commit completes, as the file's
		// commit listeners may still hold references to them
		AssFile old;
		old.Events.swap(file.Events);
		old.Styles.swap(file.Styles);

		file.Info.clear();
		file.Attachments.clear();
		file.Extradata.clear();

		for (auto const &info : snapshot.script_info)
			file.Info.emplace_back(info.first, info.second);
		for (auto const &style : snapshot.styles)
			file.Styles.push_back(*new AssStyle(style));

		file.Attachments = snapshot.attachments;

		for (auto const &event : snapshot.events)
			file.Events.push_back(*new AssDialogue(event));

		file.Extradata = snapshot.extradata;
		file.Properties = snapshot.properties;
		file.next_extradata_id = snapshot.next_extradata_id;

		applying = true;
		file.Commit("", AssFile::COMMIT_NEW);
		applying = false;
	}
};

UndoManager::UndoManager(AssFile &file)
: impl(std::make_unique<Impl>(file))
{
}

UndoManager::~UndoManager() = default;

bool UndoManager::CanUndo() const {
	return impl->undo_stack.size() > 1;
}

bool UndoManager::CanRedo() const {
	return !impl->redo_stack.empty();
}

std::string_view UndoManager::UndoDescription() const {
	if (impl->undo_stack.empty())
		return {};
	return impl->undo_stack.back().description;
}

std::string_view UndoManager::RedoDescription() const {
	if (impl->redo_stack.empty())
		return {};
	return impl->redo_stack.back().description;
}

void UndoManager::Undo() {
	if (!CanUndo())
		return;

	impl->redo_stack.splice(impl->redo_stack.end(), impl->undo_stack, std::prev(impl->undo_stack.end()));

	impl->commit_id = impl->undo_stack.back().commit_id;
	impl->Apply(impl->undo_stack.back());
}

void UndoManager::Redo() {
	if (!CanRedo())
		return;

	impl->undo_stack.splice(impl->undo_stack.end(), impl->redo_stack, std::prev(impl->redo_stack.end()));

	impl->commit_id = impl->undo_stack.back().commit_id;
	impl->Apply(impl->undo_stack.back());
}

void UndoManager::Reset() {
	impl->undo_stack.clear();
	impl->redo_stack.clear();
	impl->commit_id = 0;
	impl->next_commit_id = 1;
}

void UndoManager::SetMaxDepth(int depth) {
	impl->max_depth = std::max(depth, 2);
	impl->Trim();
}

int UndoManager::CurrentCommitId() const {
	return impl->commit_id;
}

}
