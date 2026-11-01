// Copyright (c) 2026, DanketSub contributors
//
// Permission to use, copy, modify, and distribute this software for any
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
//
// Aegisub Project http://www.aegisub.org/

#pragma once

#include <string>

class AssFile;
namespace agi { struct Context; }

/// @brief Load a default file, using the configured default resolution and
///        applying the given style catalog
/// @param file File to load into
/// @param include_dialogue_line Add a blank dialogue line to the file
/// @param style_catalog Style catalog name to fill styles from, blank to use the default style
void LoadDefaultFile(AssFile &file, bool include_dialogue_line, std::string const &style_catalog);

/// @brief Get the effective layout resolution, falling back to the video
///        resolution (or the script resolution when there is no video)
/// @param c Context
/// @param[out] lw Width
/// @param[out] lh Height
void GetEffectiveLayoutResolution(agi::Context const *c, int &lw, int &lh);
