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

#include "ass_file_ui.h"

#include "ass_file.h"
#include "ass_style.h"
#include "ass_style_storage.h"
#include "async_video_provider.h"
#include "options.h"
#include "project.h"
#include "include/aegisub/context.h"

void LoadDefaultFile(AssFile &file, bool include_dialogue_line, std::string const &style_catalog) {
	int play_res_x = 0, play_res_y = 0;
	if (!OPT_GET("Subtitle/Default Resolution/Auto")->GetBool()) {
		play_res_x = OPT_GET("Subtitle/Default Resolution/Width")->GetInt();
		play_res_y = OPT_GET("Subtitle/Default Resolution/Height")->GetInt();
	}

	file.LoadDefault(include_dialogue_line, play_res_x, play_res_y);

	// Add/replace any catalog styles requested
	if (AssStyleStorage::CatalogExists(style_catalog)) {
		AssStyleStorage catalog;
		catalog.LoadCatalog(style_catalog);
		catalog.ReplaceIntoFile(file);
	}
}

void GetEffectiveLayoutResolution(agi::Context const *c, int &lw, int &lh) {
	c->ass->GetLayoutResolution(lw, lh);
	if (lw == 0 || lh == 0) {
		if (c->project->VideoProvider()) {
			lw = c->project->VideoProvider()->GetWidth();
			lh = c->project->VideoProvider()->GetHeight();
		} else {
			c->ass->GetResolution(lw, lh);
		}
	}
}
