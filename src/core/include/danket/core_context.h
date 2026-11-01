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

#pragma once

#include "danket/ass_io.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class AssFile;

namespace danket {

class UndoManager;

/// Owns an ASS file and its undo history, with plain-text and file I/O
class CoreContext {
public:
	CoreContext();
	~CoreContext();

	CoreContext(CoreContext &&) noexcept;
	CoreContext &operator=(CoreContext &&) noexcept;

	CoreContext(CoreContext const &) = delete;
	CoreContext &operator=(CoreContext const &) = delete;

	AssFile &Subs();
	AssFile const &Subs() const;

	UndoManager &Undo();
	UndoManager const &Undo() const;

	/// Raw text of the event at the given row, empty if the row does not exist
	std::string_view GetLineText(int row) const;
	/// Set the raw text of the event at the given row; does not commit
	void SetLineText(int row, std::string_view text);

	/// Replace the current file with the parsed text on success
	/// @throws agi::Exception if the text cannot be parsed
	void LoadFromText(std::string_view text, int version = 1);

	std::string SaveToText(AssWriteOptions options = {}) const;

	/// Read a file and parse it as ASS text
	void LoadFile(std::filesystem::path const &path, int version = 1);

	/// Write the file as ASS text
	void SaveFile(std::filesystem::path const &path, AssWriteOptions options = {}) const;

private:
	std::unique_ptr<AssFile> file;
	std::unique_ptr<UndoManager> undo;
};

}
