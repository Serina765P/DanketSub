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

#include "danket/ass_io.h"

#include "ass_attachment.h"
#include "ass_dialogue.h"
#include "ass_entry.h"
#include "ass_file.h"
#include "ass_info.h"
#include "ass_parser.h"
#include "ass_style.h"

#include <libaegisub/ass/string_codec.h>
#include <libaegisub/ass/uuencode.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

char const *format_for_group(AssEntryGroup group) {
	if (group == AssEntryGroup::DIALOGUE)
		return "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text";
	if (group == AssEntryGroup::STYLE)
		return "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding";
	return nullptr;
}

std::string replace_commas(std::string str) {
	std::replace(str.begin(), str.end(), ',', ';');
	return str;
}

std::string strip_newlines(std::string str) {
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
	return str;
}

std::string format_ssa_style(AssStyle const &style) {
	std::ostringstream out;
	out << "Style: " << style.name << ',' << style.font << ',' << style.fontsize
		<< ',' << style.primary.GetSsaFormatted()
		<< ',' << style.secondary.GetSsaFormatted()
		<< ",0," << style.shadow.GetSsaFormatted()
		<< ',' << (style.bold ? -1 : 0) << ',' << (style.italic ? -1 : 0)
		<< ',' << style.borderstyle << ',' << style.outline_w << ',' << style.shadow_w
		<< ',' << AssStyle::AssToSsa(style.alignment)
		<< ',' << style.Margin[0] << ',' << style.Margin[1] << ',' << style.Margin[2]
		<< ",0," << style.encoding;
	return out.str();
}

std::string format_ssa_event(AssDialogue const &event) {
	std::ostringstream out;
	out << (event.Comment ? "Comment" : "Dialogue") << ": Marked=0,"
		<< event.Start.GetAssFormatted() << ',' << event.End.GetAssFormatted() << ','
		<< replace_commas(event.Style.get()) << ',' << replace_commas(event.Actor.get()) << ','
		<< event.Margin[0] << ',' << event.Margin[1] << ',' << event.Margin[2] << ','
		<< replace_commas(event.Effect.get()) << ',' << strip_newlines(event.Text.get());
	return out.str();
}

class AssTextWriter {
	std::string out;
	std::string_view newline;
	AssEntryGroup group = AssEntryGroup::INFO;

	void line(std::string_view text) {
		out.append(text);
		out.append(newline);
	}

	template<typename Number>
	void write_if_not_zero(char const *key, Number n) {
		if (n != Number{})
			line(std::string(key) + std::to_string(n));
	}

	void write_if_not_empty(char const *key, std::string const &value) {
		if (!value.empty())
			line(std::string(key) + value);
	}

public:
	explicit AssTextWriter(danket::AssWriteOptions options)
	: newline(options.newline)
	{
		line("[Script Info]");
		if (!options.header_comment.empty())
			line(options.header_comment);
		if (!options.header_url.empty())
			line(options.header_url);
	}

	template<typename T>
	void write_entries(T const &list) {
		for (auto const &entry : list) {
			if (entry.Group() != group) {
				// Add a blank line between each group
				line("");

				line(entry.GroupHeader());
				if (char const *format = format_for_group(entry.Group()))
					line(format);

				group = entry.Group();
			}

			line(entry.GetEntryData());
		}
	}

	void write_properties(ProjectProperties const &properties, bool write_ui_state) {
		line("");
		line("[Aegisub Project Garbage]");

		write_if_not_empty("Automation Scripts: ", properties.automation_scripts);
		write_if_not_empty("Export Filters: ", properties.export_filters);
		write_if_not_empty("Export Encoding: ", properties.export_encoding);
		write_if_not_empty("Last Style Storage: ", properties.style_storage);
		write_if_not_empty("Audio File: ", properties.audio_file);
		write_if_not_empty("Video File: ", properties.video_file);
		write_if_not_empty("Timecodes File: ", properties.timecodes_file);
		write_if_not_empty("Keyframes File: ", properties.keyframes_file);

		write_if_not_zero("Video AR Mode: ", properties.ar_mode);
		write_if_not_zero("Video AR Value: ", properties.ar_value);

		if (write_ui_state) {
			write_if_not_zero("Video Zoom Percent: ", properties.video_zoom);
			write_if_not_zero("Scroll Position: ", properties.scroll_position);
			write_if_not_zero("Active Line: ", properties.active_row);
			write_if_not_zero("Video Position: ", properties.video_position);
		}
	}

	void write_extradata(std::vector<ExtradataEntry> const &extradata) {
		if (extradata.empty())
			return;

		group = AssEntryGroup::EXTRADATA;
		line("");
		line("[Aegisub Extradata]");
		for (auto const &entry : extradata) {
			std::string data = "Data: ";
			data += std::to_string(entry.id);
			data += ",";
			data += agi::ass::inline_string_encode(entry.key);
			data += ",";
			std::string encoded = agi::ass::inline_string_encode(entry.value);
			if (4 * entry.value.size() < 3 * encoded.size()) {
				// The inline_string encoding grew the data by more than uuencoding would,
				// so base64 encode it instead
				data += "u"; // marker for uuencoding
				data += agi::ass::UUEncode(entry.value.c_str(), entry.value.c_str() + entry.value.size(), false);
			}
			else {
				data += "e"; // marker for inline_string encoding (escaping)
				data += encoded;
			}
			line(data);
		}
	}

	std::string take() { return std::move(out); }
};

}

namespace danket {

void ReadAssText(AssFile &target, std::string_view text, int version) {
	if (text.starts_with("\xEF\xBB\xBF"))
		text.remove_prefix(3);

	AssParser parser(&target, version);

	size_t pos = 0;
	while (pos <= text.size()) {
		size_t end = text.find_first_of("\r\n", pos);
		std::string line(text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos));

		if (end == std::string_view::npos) {
			pos = text.size() + 1;
		}
		else {
			pos = end + 1;
			// Treat CRLF as a single line break
			if (text[end] == '\r' && pos < text.size() && text[pos] == '\n')
				++pos;
		}

		boost::trim(line);
		if (line.starts_with("\xEF\xBB\xBF"))
			line.erase(0, 3);

		parser.AddLine(line);
	}
}

std::string WriteAssText(AssFile const &file, AssWriteOptions options) {
	AssTextWriter writer(options);
	writer.write_entries(file.Info);
	if (options.write_project_properties)
		writer.write_properties(file.Properties, options.write_ui_state);
	writer.write_entries(file.Styles);
	writer.write_entries(file.Attachments);
	writer.write_entries(file.Events);
	if (options.write_extradata)
		writer.write_extradata(file.Extradata);
	return writer.take();
}

std::string WriteSsaText(AssFile const &file, AssWriteOptions options) {
	std::string out;
	auto line = [&](std::string_view text) {
		out.append(text);
		out.append(options.newline);
	};

	line("[Script Info]");
	if (!options.header_comment.empty())
		line(options.header_comment);
	if (!options.header_url.empty())
		line(options.header_url);
	for (auto const &info : file.Info)
		line(boost::iequals(info.Key(), "scripttype") ? "ScriptType: v4.00" : info.GetEntryData());

	line("");
	line("[V4 Styles]");
	line("Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding");
	for (auto const &style : file.Styles)
		line(format_ssa_style(style));

	line("");
	line("[Fonts]");
	for (auto const &attachment : file.Attachments) {
		if (attachment.Group() == AssEntryGroup::FONT)
			line(attachment.GetEntryData());
	}

	line("");
	line("[Graphics]");
	for (auto const &attachment : file.Attachments) {
		if (attachment.Group() == AssEntryGroup::GRAPHIC)
			line(attachment.GetEntryData());
	}

	line("");
	line("[Events]");
	line("Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text");
	for (auto const &event : file.Events)
		line(format_ssa_event(event));

	return out;
}

}
