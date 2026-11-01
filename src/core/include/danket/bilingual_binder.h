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

#include <libaegisub/ass/time.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class AssDialogue;
class AssFile;

namespace danket {

/// Which half of a bilingual pair a line or logical row refers to
enum class BilingualSide { Source, Target };

/// A parsed `Effect` field binding a physical line to a bilingual row
struct SyncTag {
	std::string id;
	BilingualSide side;
};

/// One logical (possibly paired) bilingual row over one or two dialogue lines
struct BilingualRow {
	/// Empty when the logical row is an unbound single line
	std::string sync_id;
	AssDialogue *source = nullptr;
	AssDialogue *target = nullptr;
	/// True iff both source and target exist under one valid sync id
	bool linked = false;

	bool Has(BilingualSide side) const;
	AssDialogue *Line(BilingualSide side) const;
	agi::Time Start() const;
	agi::Time End() const;
};

/// Pairs source and target dialogue lines through tags stored in the standard
/// ASS `Effect` field, without introducing any non-standard script sections
class BilingualBinder {
public:
	explicit BilingualBinder(AssFile &file);

	/// Parse `sync-id:<id>;lang:src|dst`; returns nullopt for anything else
	static std::optional<SyncTag> ParseSyncTag(std::string_view effect);
	/// Build the canonical `sync-id:<id>;lang:src|dst` effect value
	static std::string FormatSyncTag(std::string_view id, BilingualSide side);
	/// First unused `row-NN` id among the valid tags currently in the file
	static std::string GenerateSyncId(AssFile const &file);

	AssFile &File() { return file; }
	std::vector<BilingualRow> const &Rows() const { return rows; }
	/// Rebuild the logical rows from the physical dialogue lines
	void Refresh();

	bool SetTime(int row, agi::Time start, agi::Time end);
	bool SetText(int row, BilingualSide side, std::string_view text);
	bool DeleteSide(int row, BilingualSide side);
	bool DeleteRow(int row);
	bool Unbind(int row);
	int AddPair(agi::Time start, agi::Time end,
		std::string_view source_text = {},
		std::string_view target_text = {},
		std::string_view source_style = "Default",
		std::string_view target_style = "Default");

private:
	AssFile &file;
	std::vector<BilingualRow> rows;
};

}
