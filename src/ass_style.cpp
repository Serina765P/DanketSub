// Copyright (c) 2005, Rodrigo Braz Monteiro
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

/// @file ass_style.cpp
/// @brief Class for style definitions in subtitles
/// @ingroup subs_storage
///

#include "ass_style.h"

#include "ass_core_error.h"

#include <libaegisub/split.h>

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <string>
#include <vector>

AssStyle::AssStyle() {
	std::fill(Margin.begin(), Margin.end(), 10);

	UpdateData();
}

AssEntryGroup AssStyle::Group() const { return AssEntryGroup::STYLE; }

namespace {
class parser {
	agi::split_iterator<char> pos;

	std::string_view next_tok() {
		if (pos.eof())
			throw SubtitleFormatParseError("Malformed style: not enough fields");
		return agi::Trim(*pos++);
	}

public:
	parser(std::string_view str) {
		if (auto colon = str.find(':'); colon != str.npos) {
			str.remove_prefix(colon + 1);
			pos = agi::Split(str, ',');
		}
	}

	void check_done() const {
		if (!pos.eof())
			throw SubtitleFormatParseError("Malformed style: too many fields");
	}

	std::string next_str() { return std::string(next_tok()); }
	agi::Color next_color() { return next_tok(); }

	int next_int() {
		try {
			return boost::lexical_cast<int>(next_tok());
		}
		catch (boost::bad_lexical_cast const&) {
			throw SubtitleFormatParseError("Malformed style: bad int field");
		}
	}

	double next_double() {
		try {
			return boost::lexical_cast<double>(next_tok());
		}
		catch (boost::bad_lexical_cast const&) {
			throw SubtitleFormatParseError("Malformed style: bad double field");
		}
	}

	void skip_token() {
		if (!pos.eof())
			++pos;
	}
};
}

AssStyle::AssStyle(std::string_view str, int version) {
	parser p(str);

	name = p.next_str();
	font = p.next_str();
	fontsize = p.next_double();

	if (version != 0) {
		primary = p.next_color();
		secondary = p.next_color();
		outline = p.next_color();
		shadow = p.next_color();
	}
	else {
		primary = p.next_color();
		secondary = p.next_color();

		// Skip tertiary color
		p.skip_token();

		// Read shadow/outline color
		outline = p.next_color();
		shadow = outline;
	}

	bold = !!p.next_int();
	italic = !!p.next_int();

	if (version != 0) {
		underline = !!p.next_int();
		strikeout = !!p.next_int();

		scalex = p.next_double();
		scaley = p.next_double();
		spacing = p.next_double();
		angle = p.next_double();
	}
	else {
		// SSA defaults
		underline = false;
		strikeout = false;

		scalex = 100;
		scaley = 100;
		spacing = 0;
		angle = 0.0;
	}

	borderstyle = p.next_int();
	outline_w = p.next_double();
	shadow_w = p.next_double();
	alignment = p.next_int();

	if (version == 0)
		alignment = SsaToAss(alignment);

	Margin[0] = std::clamp(p.next_int(), -9999, 99999);
	Margin[1] = std::clamp(p.next_int(), -9999, 99999);
	Margin[2] = std::clamp(p.next_int(), -9999, 99999);

	// Skip alpha level
	if (version == 0)
		p.skip_token();

	encoding = p.next_int();

	p.check_done();

	UpdateData();
}

void AssStyle::UpdateData() {
	std::replace(name.begin(), name.end(), ',', ';');
	std::replace(font.begin(), font.end(), ',', ';');

	std::ostringstream out;
	out << "Style: " << name << ',' << font << ',' << fontsize
		<< ',' << primary.GetAssStyleFormatted()
		<< ',' << secondary.GetAssStyleFormatted()
		<< ',' << outline.GetAssStyleFormatted()
		<< ',' << shadow.GetAssStyleFormatted()
		<< ',' << (bold ? -1 : 0) << ',' << (italic ? -1 : 0)
		<< ',' << (underline ? -1 : 0) << ',' << (strikeout ? -1 : 0)
		<< ',' << scalex << ',' << scaley << ',' << spacing << ',' << angle
		<< ',' << borderstyle << ',' << outline_w << ',' << shadow_w << ',' << alignment
		<< ',' << Margin[0] << ',' << Margin[1] << ',' << Margin[2] << ',' << encoding;
	data = out.str();
}

std::vector<AssEncoding> AssStyle::GetEncodings() {
	return {
		{ -1,  "Auto-detect base direction (libass only)" },
		{ 0,   "ANSI" },
		{ 1,   "Default" },
		{ 2,   "Symbol" },
		{ 77,  "Mac" },
		{ 128, "Shift_JIS" },
		{ 129, "Hangeul" },
		{ 130, "Johab" },
		{ 134, "GB2312" },
		{ 136, "Chinese BIG5" },
		{ 161, "Greek" },
		{ 162, "Turkish" },
		{ 163, "Vietnamese" },
		{ 177, "Hebrew" },
		{ 178, "Arabic" },
		{ 186, "Baltic" },
		{ 204, "Russian" },
		{ 222, "Thai" },
		{ 238, "East European" },
		{ 255, "OEM" },
	};
}

int AssStyle::AssToSsa(int ass_align) {
	switch (ass_align) {
		case 1:  return 1;
		case 2:  return 2;
		case 3:  return 3;
		case 4:  return 9;
		case 5:  return 10;
		case 6:  return 11;
		case 7:  return 5;
		case 8:  return 6;
		case 9:  return 7;
		default: return 2;
	}
}

int AssStyle::SsaToAss(int ssa_align) {
	switch(ssa_align) {
		case 1:  return 1;
		case 2:  return 2;
		case 3:  return 3;
		case 5:  return 7;
		case 6:  return 8;
		case 7:  return 9;
		case 9:  return 4;
		case 10: return 5;
		case 11: return 6;
		default: return 2;
	}
}
