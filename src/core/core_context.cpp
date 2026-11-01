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

#include "danket/core_context.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "danket/ass_io.h"
#include "danket/undo_manager.h"

#include <libaegisub/fs.h>

#include <fstream>
#include <iterator>
#include <utility>

namespace danket {

CoreContext::CoreContext()
: file(std::make_unique<AssFile>())
, undo(std::make_unique<UndoManager>(*file))
{
	// Establish the baseline state so that the first edit can be undone
	file->Commit("", AssFile::COMMIT_NEW);
}

CoreContext::~CoreContext() = default;

CoreContext::CoreContext(CoreContext &&) noexcept = default;
CoreContext &CoreContext::operator=(CoreContext &&) noexcept = default;

AssFile &CoreContext::Subs() {
	return *file;
}

AssFile const &CoreContext::Subs() const {
	return *file;
}

UndoManager &CoreContext::Undo() {
	return *undo;
}

UndoManager const &CoreContext::Undo() const {
	return *undo;
}

std::string_view CoreContext::GetLineText(int row) const {
	for (auto const &line : file->Events) {
		if (line.Row == row)
			return line.GetText();
	}
	return {};
}

void CoreContext::SetLineText(int row, std::string_view text) {
	for (auto &line : file->Events) {
		if (line.Row == row) {
			line.SetText(text);
			return;
		}
	}
}

void CoreContext::LoadFromText(std::string_view text, int version) {
	// Parse into a temporary so that a malformed script leaves the current
	// file untouched
	auto parsed = std::make_unique<AssFile>();
	ReadAssText(*parsed, text, version);

	file->swap(*parsed);

	undo->Reset();
	file->Commit("", AssFile::COMMIT_NEW);
}

std::string CoreContext::SaveToText(AssWriteOptions options) const {
	return WriteAssText(*file, options);
}

void CoreContext::LoadFile(std::filesystem::path const &path, int version) {
	agi::fs::path file_path(path);
	std::ifstream in(file_path, std::ios::binary);
	if (!in)
		throw agi::fs::FileNotFound(file_path);

	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (in.bad())
		throw agi::fs::FileNotAccessible(file_path);

	LoadFromText(text, version);
}

void CoreContext::SaveFile(std::filesystem::path const &path, AssWriteOptions options) const {
	agi::fs::path file_path(path);
	std::string text = SaveToText(options);

	std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
	if (!out)
		throw agi::fs::FileNotAccessible(file_path);

	out.write(text.data(), static_cast<std::streamsize>(text.size()));
	if (!out)
		throw agi::fs::FileNotAccessible(file_path);
}

}
