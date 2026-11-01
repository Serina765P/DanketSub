// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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

#include <libaegisub/ass/smpte.h>
#include <libaegisub/ass/time.h>

#include <libaegisub/format.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>

#include <string>
#include <utility>
#include <vector>

namespace agi {
SmpteFormatter::SmpteFormatter(vfr::Framerate fps, char sep)
: fps(std::move(fps))
, sep(sep)
{
}

std::string SmpteFormatter::ToSMPTE(Time time) const {
	int h=0, m=0, s=0, f=0;
	fps.SmpteAtTime(time, &h, &m, &s, &f);
	return format("%02d%c%02d%c%02d%c%02d", h, sep, m, sep, s, sep, f);
}

Time SmpteFormatter::FromSMPTE(std::string const& str) const {
	std::vector<std::string> toks;
	Split(toks, str, sep);
	if (toks.size() != 4) return 0;

	int h, m, s, f;
	util::try_parse(toks[0], &h);
	util::try_parse(toks[1], &m);
	util::try_parse(toks[2], &s);
	util::try_parse(toks[3], &f);
	return fps.TimeAtSmpte(h, m, s, f);
}
}
