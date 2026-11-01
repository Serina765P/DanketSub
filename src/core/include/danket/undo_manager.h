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

#include <memory>
#include <string_view>

class AssFile;

namespace danket {

/// Snapshot-based undo stack driven by AssFile::Commit
class UndoManager {
public:
	/// Connect to the file's commit signal
	explicit UndoManager(AssFile &file);
	~UndoManager();

	UndoManager(UndoManager const &) = delete;
	UndoManager &operator=(UndoManager const &) = delete;
	UndoManager(UndoManager &&) = delete;
	UndoManager &operator=(UndoManager &&) = delete;

	bool CanUndo() const;
	bool CanRedo() const;

	/// Description of the top undo/redo snapshot, empty if there is none.
	/// Like SubsController, this is the description of the change that
	/// produced the snapshot's state rather than the change that a call to
	/// Undo() or Redo() would apply.
	std::string_view UndoDescription() const;
	std::string_view RedoDescription() const;

	/// Restore the previous/next snapshot. No-op if there is none.
	void Undo();
	void Redo();

	/// Clear both stacks, reaching a state where there is no history
	void Reset();

	/// Maximum number of retained snapshots; values below 2 are clamped to 2
	void SetMaxDepth(int depth);

	/// Identifier of the most recently applied commit
	int CurrentCommitId() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
