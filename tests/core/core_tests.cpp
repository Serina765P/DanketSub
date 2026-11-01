// Copyright (c) 2026, DanketSub contributors
//
// Permission to use, copy, modify, and/or distribute this software for any
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

/// @file core_tests.cpp
/// @brief Self-contained tests for the GUI-free DanketSub core library
///
/// Run from tests/core (CTest) or tests (Meson); otherwise the sample is
/// located relative to this source file.

#include <danket/ass.h>
#include <danket/ass/ass_dialogue.h>
#include <danket/ass/ass_file.h>
#include <danket/ass/ass_info.h>
#include <danket/ass/ass_style.h>
#include <danket/ass_io.h>
#include <danket/core_context.h>
#include <danket/undo_manager.h>

#include <libaegisub/ass/time.h>
#include <libaegisub/exception.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

struct TestFailure final : std::runtime_error {
	using std::runtime_error::runtime_error;
};

void report_failure(char const *file, int line, std::string const &message) {
	std::cout << "  FAIL " << file << ":" << line << ": " << message << "\n";
	++failures;
}

void report_uncaught(std::string const &message) {
	std::cout << "  FAIL " << message << "\n";
	++failures;
}

template<typename T>
std::string describe_value(T const &value) {
	using U = std::decay_t<T>;
	if constexpr (std::is_convertible_v<U const &, std::string_view>)
		return "'" + std::string(std::string_view(value)) + "'";
	else if constexpr (std::is_enum_v<U>)
		return std::to_string(static_cast<long long>(value));
	else {
		std::ostringstream out;
		out << value;
		return out.str();
	}
}

template<typename A, typename B>
void check_equal(char const *expression, A const &actual, B const &expected, char const *file, int line) {
	if (!(actual == expected))
		report_failure(file, line, std::string(expression) + ": expected " + describe_value(expected) + ", got " + describe_value(actual));
}

void check_near(char const *expression, double actual, double expected, double tolerance, char const *file, int line) {
	if (!(std::abs(actual - expected) <= tolerance))
		report_failure(file, line, std::string(expression) + ": expected " + describe_value(expected) + ", got " + describe_value(actual));
}

void check_contains(std::string_view haystack, std::string_view needle, char const *expression, char const *file, int line) {
	if (haystack.find(needle) == std::string_view::npos) {
		std::string displayed(haystack.substr(0, 4000));
		report_failure(file, line, std::string(expression) + ": missing " + describe_value(needle) + "\n--- haystack ---\n" + displayed + "\n--- end ---");
	}
}

void check_not_contains(std::string_view haystack, std::string_view needle, char const *expression, char const *file, int line) {
	if (haystack.find(needle) != std::string_view::npos)
		report_failure(file, line, std::string(expression) + ": unexpectedly found " + describe_value(needle));
}

#define CHECK(condition) do { if (!(condition)) ::report_failure(__FILE__, __LINE__, #condition); } while (0)
#define CHECK_EQ(actual, expected) ::check_equal(#actual " == " #expected, (actual), (expected), __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) ::check_near(#actual " ~= " #expected, (actual), (expected), (tolerance), __FILE__, __LINE__)
#define CHECK_CONTAINS(haystack, needle) ::check_contains((haystack), (needle), #haystack " contains " #needle, __FILE__, __LINE__)
#define CHECK_NOT_CONTAINS(haystack, needle) ::check_not_contains((haystack), (needle), #haystack " does not contain " #needle, __FILE__, __LINE__)
#define REQUIRE(condition) do { if (!(condition)) { ::report_failure(__FILE__, __LINE__, std::string("required: ") + #condition); throw ::TestFailure(#condition); } } while (0)

struct TestCase {
	char const *name;
	void (*run)();
};

std::vector<TestCase> &all_tests() {
	static std::vector<TestCase> tests;
	return tests;
}

struct TestRegistration {
	TestRegistration(char const *name, void (*run)()) { all_tests().push_back({name, run}); }
};

#define TEST(name) \
	static void test_##name(); \
	static ::TestRegistration registration_##name(#name, &test_##name); \
	static void test_##name()

// ---------------------------------------------------------------------------
// Fixtures and helpers
// ---------------------------------------------------------------------------

fs::path sample_path() {
	fs::path const candidates[] = {
		fs::path("data") / "sample.ass",
		fs::path("core") / "data" / "sample.ass",
		fs::path("tests") / "core" / "data" / "sample.ass",
		fs::path(__FILE__).parent_path() / "data" / "sample.ass",
	};
	for (auto const &candidate : candidates) {
		std::error_code error;
		if (fs::is_regular_file(candidate, error))
			return candidate;
	}
	throw std::runtime_error("could not locate tests/core/data/sample.ass");
}

std::string read_file(fs::path const &path) {
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("could not open " + path.string() + " for reading");
	std::ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

std::string sample_text() {
	return read_file(sample_path());
}

struct TempDir {
	fs::path path;

	TempDir() {
		static std::atomic<unsigned long long> counter{0};
		auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = fs::temp_directory_path() / ("danket_core_tests_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
		std::error_code error;
		fs::create_directories(path, error);
		if (error)
			throw std::runtime_error("could not create temporary directory: " + error.message());
	}

	~TempDir() {
		std::error_code error;
		fs::remove_all(path, error);
	}

	TempDir(TempDir const &) = delete;
	TempDir &operator=(TempDir const &) = delete;

	fs::path file(std::string_view name) const { return path / std::string(name); }
};

danket::AssWriteOptions options_lf() {
	danket::AssWriteOptions options;
	options.newline = "\n";
	return options;
}

AssDialogue &event_at(danket::CoreContext &context, int row) {
	for (auto &line : context.Subs().Events) {
		if (line.Row == row)
			return line;
	}
	throw std::runtime_error("no dialogue event at row " + std::to_string(row));
}

std::size_t position_of(std::string_view haystack, std::string_view needle) {
	auto position = haystack.find(needle);
	if (position == std::string_view::npos)
		throw std::runtime_error("missing section marker: " + std::string(needle));
	return position;
}

std::string replace_all(std::string text, std::string_view from, std::string_view to) {
	std::size_t position = 0;
	while ((position = text.find(from, position)) != std::string::npos) {
		text.replace(position, from.size(), to);
		position += to.size();
	}
	return text;
}

bool all_newlines_are_crlf(std::string const &text) {
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r'))
			return false;
	}
	return true;
}

void expect_same_model(AssFile const &actual, AssFile const &expected) {
	CHECK_EQ(actual.Info.size(), expected.Info.size());
	std::size_t const info_count = std::min(actual.Info.size(), expected.Info.size());
	for (std::size_t i = 0; i < info_count; ++i) {
		CHECK_EQ(actual.Info[i].Key(), expected.Info[i].Key());
		CHECK_EQ(actual.Info[i].Value(), expected.Info[i].Value());
	}

	CHECK_EQ(actual.Styles.size(), expected.Styles.size());
	auto actual_style = actual.Styles.begin();
	auto expected_style = expected.Styles.begin();
	for (; actual_style != actual.Styles.end() && expected_style != expected.Styles.end(); ++actual_style, ++expected_style) {
		CHECK_EQ(actual_style->name, expected_style->name);
		CHECK_EQ(actual_style->font, expected_style->font);
		CHECK_NEAR(actual_style->fontsize, expected_style->fontsize, 1e-9);
		CHECK(actual_style->primary == expected_style->primary);
		CHECK(actual_style->secondary == expected_style->secondary);
		CHECK(actual_style->outline == expected_style->outline);
		CHECK(actual_style->shadow == expected_style->shadow);
		CHECK_EQ(actual_style->bold, expected_style->bold);
		CHECK_EQ(actual_style->italic, expected_style->italic);
		CHECK_EQ(actual_style->underline, expected_style->underline);
		CHECK_EQ(actual_style->strikeout, expected_style->strikeout);
		CHECK_NEAR(actual_style->scalex, expected_style->scalex, 1e-9);
		CHECK_NEAR(actual_style->scaley, expected_style->scaley, 1e-9);
		CHECK_NEAR(actual_style->spacing, expected_style->spacing, 1e-9);
		CHECK_NEAR(actual_style->angle, expected_style->angle, 1e-9);
		CHECK_EQ(actual_style->borderstyle, expected_style->borderstyle);
		CHECK_NEAR(actual_style->outline_w, expected_style->outline_w, 1e-9);
		CHECK_NEAR(actual_style->shadow_w, expected_style->shadow_w, 1e-9);
		CHECK_EQ(actual_style->alignment, expected_style->alignment);
		CHECK_EQ(actual_style->Margin[0], expected_style->Margin[0]);
		CHECK_EQ(actual_style->Margin[1], expected_style->Margin[1]);
		CHECK_EQ(actual_style->Margin[2], expected_style->Margin[2]);
		CHECK_EQ(actual_style->encoding, expected_style->encoding);
		CHECK_EQ(actual_style->GetEntryData(), expected_style->GetEntryData());
	}

	CHECK_EQ(actual.Events.size(), expected.Events.size());
	auto actual_event = actual.Events.begin();
	auto expected_event = expected.Events.begin();
	for (; actual_event != actual.Events.end() && expected_event != expected.Events.end(); ++actual_event, ++expected_event) {
		CHECK_EQ(actual_event->Comment, expected_event->Comment);
		CHECK_EQ(actual_event->Layer, expected_event->Layer);
		CHECK_EQ(int(actual_event->Start), int(expected_event->Start));
		CHECK_EQ(int(actual_event->End), int(expected_event->End));
		CHECK_EQ(actual_event->Style.get(), expected_event->Style.get());
		CHECK_EQ(actual_event->Actor.get(), expected_event->Actor.get());
		CHECK_EQ(actual_event->Effect.get(), expected_event->Effect.get());
		CHECK_EQ(actual_event->Margin[0], expected_event->Margin[0]);
		CHECK_EQ(actual_event->Margin[1], expected_event->Margin[1]);
		CHECK_EQ(actual_event->Margin[2], expected_event->Margin[2]);
		CHECK_EQ(actual_event->GetText(), expected_event->GetText());
	}

	CHECK_EQ(actual.Extradata.size(), expected.Extradata.size());
	std::size_t const extradata_count = std::min(actual.Extradata.size(), expected.Extradata.size());
	for (std::size_t i = 0; i < extradata_count; ++i) {
		CHECK_EQ(actual.Extradata[i].id, expected.Extradata[i].id);
		CHECK_EQ(actual.Extradata[i].key, expected.Extradata[i].key);
		CHECK_EQ(actual.Extradata[i].value, expected.Extradata[i].value);
	}
	CHECK_EQ(actual.next_extradata_id, expected.next_extradata_id);

	CHECK_EQ(actual.Properties.automation_scripts, expected.Properties.automation_scripts);
	CHECK_EQ(actual.Properties.export_filters, expected.Properties.export_filters);
	CHECK_EQ(actual.Properties.export_encoding, expected.Properties.export_encoding);
	CHECK_EQ(actual.Properties.style_storage, expected.Properties.style_storage);
	CHECK_EQ(actual.Properties.audio_file, expected.Properties.audio_file);
	CHECK_EQ(actual.Properties.video_file, expected.Properties.video_file);
	CHECK_EQ(actual.Properties.timecodes_file, expected.Properties.timecodes_file);
	CHECK_EQ(actual.Properties.keyframes_file, expected.Properties.keyframes_file);
	CHECK(actual.Properties.automation_settings == expected.Properties.automation_settings);
	CHECK_NEAR(actual.Properties.video_zoom, expected.Properties.video_zoom, 1e-9);
	CHECK_NEAR(actual.Properties.ar_value, expected.Properties.ar_value, 1e-9);
	CHECK_EQ(actual.Properties.scroll_position, expected.Properties.scroll_position);
	CHECK_EQ(actual.Properties.active_row, expected.Properties.active_row);
	CHECK_EQ(actual.Properties.ar_mode, expected.Properties.ar_mode);
	CHECK_EQ(actual.Properties.video_position, expected.Properties.video_position);
}

// ---------------------------------------------------------------------------
// Tests: loading the sample fixture
// ---------------------------------------------------------------------------

TEST(load_sample_file) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	AssFile &file = context.Subs();

	CHECK_EQ(file.Info.size(), std::size_t(10));
	CHECK_EQ(file.Styles.size(), std::size_t(2));
	CHECK_EQ(file.Events.size(), std::size_t(6));
	CHECK_EQ(file.Extradata.size(), std::size_t(2));

	CHECK_EQ(file.GetScriptInfo("Title"), "Core Test Sample");
	CHECK_EQ(file.GetScriptInfo("ScriptType"), "v4.00+");
	CHECK_EQ(file.GetScriptInfo("WrapStyle"), "0");
	CHECK_EQ(file.GetScriptInfo("ScaledBorderAndShadow"), "yes");
	CHECK_EQ(file.GetScriptInfo("YCbCr Matrix"), "TV.709");
	CHECK_EQ(file.GetScriptInfoAsInt("PlayResX"), 1920);
	CHECK_EQ(file.GetScriptInfoAsInt("PlayResY"), 1080);
	CHECK_EQ(file.GetScriptInfoAsInt("LayoutResX"), 1920);
	CHECK_EQ(file.GetScriptInfoAsInt("LayoutResY"), 1080);

	CHECK_EQ(file.Properties.audio_file, "audio/sample.wav");
	CHECK_EQ(file.Properties.video_file, "video/sample.mp4");
	CHECK_EQ(file.Properties.ar_mode, 2);
	CHECK_NEAR(file.Properties.ar_value, 1.777778, 1e-9);
	CHECK_NEAR(file.Properties.video_zoom, 1.5, 1e-9);
	CHECK_EQ(file.Properties.scroll_position, 7);
	CHECK_EQ(file.Properties.active_row, 3);
	CHECK_EQ(file.Properties.video_position, 4);

	auto style = file.Styles.begin();
	REQUIRE(style != file.Styles.end());
	CHECK_EQ(style->name, "Default");
	CHECK_EQ(style->font, "Arial");
	CHECK_NEAR(style->fontsize, 48.0, 1e-9);
	CHECK(style->bold);
	CHECK(!style->italic);
	CHECK_EQ(style->alignment, 2);
	CHECK_EQ(style->Margin[0], 10);
	CHECK_EQ(style->Margin[1], 10);
	CHECK_EQ(style->Margin[2], 10);
	CHECK_EQ(style->encoding, 1);
	++style;
	REQUIRE(style != file.Styles.end());
	CHECK_EQ(style->name, "Sign");
	CHECK_EQ(style->font, "Open Sans");
	CHECK_NEAR(style->fontsize, 72.0, 1e-9);
	CHECK(!style->bold);
	CHECK(style->italic);
	CHECK_NEAR(style->scalex, 105.0, 1e-9);
	CHECK_NEAR(style->spacing, 1.5, 1e-9);
	CHECK_EQ(style->alignment, 8);
	CHECK_NEAR(style->outline_w, 3.0, 1e-9);
	CHECK_NEAR(style->shadow_w, 1.0, 1e-9);
	CHECK_EQ(style->Margin[2], 60);

	CHECK(!event_at(context, 0).Comment);
	CHECK_EQ(event_at(context, 0).Layer, 0);
	CHECK_EQ(int(event_at(context, 0).Start), 1000);
	CHECK_EQ(int(event_at(context, 0).End), 3500);
	CHECK_EQ(event_at(context, 0).Style.get(), "Default");
	CHECK_EQ(event_at(context, 0).Actor.get(), "Narrator");
	CHECK_EQ(event_at(context, 0).GetText(), "{\\fad(200,200)}It was a dark and stormy night...");
	CHECK_CONTAINS(event_at(context, 0).GetText(), "{\\fad(200,200)}");

	CHECK_EQ(event_at(context, 1).Layer, 1);
	CHECK_EQ(int(event_at(context, 1).Start), 3500);
	CHECK_EQ(int(event_at(context, 1).End), 5000);
	CHECK_EQ(event_at(context, 1).Style.get(), "Sign");
	CHECK_EQ(event_at(context, 1).Actor.get(), "Alice");
	CHECK_EQ(event_at(context, 1).Margin[0], 10);
	CHECK_EQ(event_at(context, 1).Margin[1], 20);
	CHECK_EQ(event_at(context, 1).Margin[2], 30);
	CHECK_EQ(event_at(context, 1).GetText(), "{\\pos(960,200)}Beware the {\\i1}clever{\\i0} fox");
	CHECK_CONTAINS(event_at(context, 1).GetText(), "{\\pos(960,200)}");
	CHECK_CONTAINS(event_at(context, 1).GetText(), "{\\i1}");

	CHECK(event_at(context, 2).Comment);
	CHECK_EQ(int(event_at(context, 2).Start), 5000);
	CHECK_EQ(int(event_at(context, 2).End), 6000);
	CHECK_EQ(event_at(context, 2).GetText(), "{\\an8}This line is a comment");

	CHECK_EQ(event_at(context, 3).Layer, 2);
	CHECK_EQ(event_at(context, 3).Style.get(), "Default");
	CHECK_EQ(event_at(context, 3).Effect.get(), "fade");
	CHECK_EQ(event_at(context, 3).GetText(), "{\\k25}Ka{\\k30}ra{\\k45}o{\\k20}ke");
	CHECK_CONTAINS(event_at(context, 3).GetText(), "{\\k25}");

	CHECK_EQ(int(event_at(context, 4).Start), 7250);
	CHECK_EQ(int(event_at(context, 4).End), 9000);
	CHECK_EQ(event_at(context, 4).GetText(), "{\\p1}m 0 0 l 100 0 100 100 0 100{\\p0}");
	CHECK_CONTAINS(event_at(context, 4).GetText(), "{\\p1}");

	CHECK_EQ(int(event_at(context, 5).Start), 9000);
	CHECK_EQ(int(event_at(context, 5).End), 10500);
	CHECK_EQ(event_at(context, 5).GetText(), "Line with \\N newline{\\blur2} and {\\c&HFF0000&}color");
	CHECK_CONTAINS(event_at(context, 5).GetText(), "{\\blur2}");
	CHECK_CONTAINS(event_at(context, 5).GetText(), "{\\c&HFF0000&}");

	CHECK_EQ(file.Extradata[0].id, 0u);
	CHECK_EQ(file.Extradata[0].key, "core.test");
	CHECK_EQ(file.Extradata[0].value, "Hello, world: 100%|done");
	CHECK_EQ(file.Extradata[1].id, 7u);
	CHECK_EQ(file.Extradata[1].key, "core.bin");
	CHECK_EQ(file.Extradata[1].value, "abc");
	CHECK_EQ(file.next_extradata_id, 8u);

	CHECK_EQ(context.GetLineText(0), "{\\fad(200,200)}It was a dark and stormy night...");
	CHECK_EQ(context.GetLineText(2), "{\\an8}This line is a comment");
	CHECK_EQ(context.GetLineText(5), "Line with \\N newline{\\blur2} and {\\c&HFF0000&}color");

	CHECK(!context.Undo().CanUndo());
	CHECK(!context.Undo().CanRedo());
	CHECK_EQ(context.Undo().CurrentCommitId(), 1);
}

// ---------------------------------------------------------------------------
// Tests: exact writer output and timing format
// ---------------------------------------------------------------------------

TEST(writer_output_is_exact) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	std::string const out = context.SaveToText(options_lf());
	REQUIRE(!out.empty());

	CHECK(out.starts_with("[Script Info]\nTitle: Core Test Sample\nOriginal Script: DanketSub Tests\nScriptType: v4.00+\nWrapStyle: 0\nPlayResX: 1920\nPlayResY: 1080\nScaledBorderAndShadow: yes\nYCbCr Matrix: TV.709\nLayoutResX: 1920\nLayoutResY: 1080\n"));
	CHECK_CONTAINS(out, "\n[Aegisub Project Garbage]\nAudio File: audio/sample.wav\nVideo File: video/sample.mp4\nVideo AR Mode: 2\nVideo AR Value: 1.777778\nVideo Zoom Percent: 1.500000\nScroll Position: 7\nActive Line: 3\nVideo Position: 4\n");

	CHECK_CONTAINS(out, "\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");
	CHECK_CONTAINS(out, "Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n");
	CHECK_CONTAINS(out, "Style: Sign,Open Sans,72,&H00FFD24A,&H000000FF,&H00201810,&H80000000,0,-1,0,0,105,100,1.5,0,1,3,1,8,40,40,60,1\n");

	CHECK_CONTAINS(out, "\n[Fonts]\nfontname: sample.ttf");
	CHECK_CONTAINS(out, "97*D");

	CHECK_CONTAINS(out, "\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n");
	CHECK_CONTAINS(out, "Dialogue: 0,0:00:01.00,0:00:03.50,Default,Narrator,0,0,0,,{\\fad(200,200)}It was a dark and stormy night...\n");
	CHECK_CONTAINS(out, "Dialogue: 1,0:00:03.50,0:00:05.00,Sign,Alice,10,20,30,,{\\pos(960,200)}Beware the {\\i1}clever{\\i0} fox\n");
	CHECK_CONTAINS(out, "Comment: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,,{\\an8}This line is a comment\n");
	CHECK_CONTAINS(out, "Dialogue: 2,0:00:06.00,0:00:07.25,Default,Bob,0,0,0,fade,{\\k25}Ka{\\k30}ra{\\k45}o{\\k20}ke\n");
	CHECK_CONTAINS(out, "Dialogue: 0,0:00:07.25,0:00:09.00,Sign,,0,0,0,,{\\p1}m 0 0 l 100 0 100 100 0 100{\\p0}\n");
	CHECK_CONTAINS(out, "Dialogue: 0,0:00:09.00,0:00:10.50,Default,,0,0,0,,Line with \\N newline{\\blur2} and {\\c&HFF0000&}color\n");

	CHECK_CONTAINS(out, "\n[Aegisub Extradata]\nData: 0,core.test,eHello#2C world#3A 100%#7Cdone\nData: 7,core.bin,eabc\n");
	CHECK_EQ(out.back(), '\n');

	CHECK(position_of(out, "[Script Info]") < position_of(out, "[Aegisub Project Garbage]"));
	CHECK(position_of(out, "[Aegisub Project Garbage]") < position_of(out, "[V4+ Styles]"));
	CHECK(position_of(out, "[V4+ Styles]") < position_of(out, "[Fonts]"));
	CHECK(position_of(out, "[Fonts]") < position_of(out, "[Events]"));
	CHECK(position_of(out, "[Events]") < position_of(out, "[Aegisub Extradata]"));

	CHECK_EQ(agi::Time(1000).GetAssFormatted(), "0:00:01.00");
	CHECK_EQ(agi::Time(3500).GetAssFormatted(), "0:00:03.50");
	CHECK_EQ(agi::Time(3723450).GetAssFormatted(), "1:02:03.45");
	CHECK_EQ(agi::Time("0:00:01.00").GetAssFormatted(), "0:00:01.00");
}

// ---------------------------------------------------------------------------
// Tests: editing text and timing through the public API
// ---------------------------------------------------------------------------

TEST(write_ssa) {
	danket::CoreContext context;
	context.LoadFile(sample_path());

	danket::AssWriteOptions options = options_lf();
	options.header_comment = "; Script generated by DanketSub tests";
	options.header_url = "; https://aegisub.org/";
	std::string const ssa = danket::WriteSsaText(context.Subs(), options);

	CHECK(ssa.starts_with("[Script Info]\n; Script generated by DanketSub tests\n; https://aegisub.org/\nTitle: Core Test Sample\n"));
	CHECK_CONTAINS(ssa, "ScriptType: v4.00\n");
	CHECK_NOT_CONTAINS(ssa, "ScriptType: v4.00+\n");
	CHECK_CONTAINS(ssa, "[V4 Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n");
	CHECK_CONTAINS(ssa, "Style: Default,Arial,48,16777215,255,0,0,-1,0,1,2,2,2,10,10,10,0,1\n");
	CHECK_CONTAINS(ssa, "Style: Sign,Open Sans,72,16765514,255,0,-2147483648,0,-1,1,3,1,6,40,40,60,0,1\n");
	CHECK_CONTAINS(ssa, "[Fonts]\nfontname: sample.ttf");
	CHECK_CONTAINS(ssa, "97*D");
	CHECK_CONTAINS(ssa, "[Graphics]\n\n[Events]\n");
	CHECK_CONTAINS(ssa, "Dialogue: Marked=0,0:00:01.00,0:00:03.50,Default,Narrator,0,0,0,,{\\fad(200,200)}It was a dark and stormy night...\n");
	CHECK_CONTAINS(ssa, "Comment: Marked=0,0:00:05.00,0:00:06.00,Default,,0,0,0,,{\\an8}This line is a comment\n");
	CHECK_CONTAINS(ssa, "Dialogue: Marked=0,0:00:09.00,0:00:10.50,Default,,0,0,0,,Line with \\N newline{\\blur2} and {\\c&HFF0000&}color\n");
}

TEST(modify_text_and_times) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	CHECK(!context.Undo().CanUndo());

	std::string const original(context.GetLineText(0));
	std::string_view const replacement = "{\\b1}Modified through string_view{\\b0}";
	context.SetLineText(0, replacement);
	CHECK_EQ(context.GetLineText(0), std::string(replacement));
	CHECK(original != std::string(replacement));
	CHECK(!context.Undo().CanUndo());

	event_at(context, 0).Start = agi::Time(1000);
	event_at(context, 0).End = agi::Time(2500);
	event_at(context, 5).Start = agi::Time(3723450);
	event_at(context, 5).End = agi::Time(3725000);

	int const commit_id = context.Subs().Commit("edit text and timing", AssFile::COMMIT_DIAG_TEXT | AssFile::COMMIT_DIAG_TIME, -1);
	CHECK_EQ(context.Undo().CurrentCommitId(), commit_id);
	CHECK_EQ(context.Undo().UndoDescription(), "edit text and timing");
	CHECK(context.Undo().CanUndo());

	std::string const written = context.SaveToText(options_lf());
	CHECK_CONTAINS(written, "Dialogue: 0,0:00:01.00,0:00:02.50,Default,Narrator,0,0,0,,{\\b1}Modified through string_view{\\b0}\n");
	CHECK_CONTAINS(written, "Dialogue: 0,1:02:03.45,1:02:05.00,Default,,0,0,0,,Line with \\N newline{\\blur2} and {\\c&HFF0000&}color\n");

	danket::CoreContext reparsed;
	reparsed.LoadFromText(written);
	CHECK_EQ(reparsed.GetLineText(0), std::string(replacement));
	CHECK_EQ(int(event_at(reparsed, 0).Start), 1000);
	CHECK_EQ(int(event_at(reparsed, 0).End), 2500);
	CHECK_EQ(int(event_at(reparsed, 5).Start), 3723450);
	CHECK_EQ(int(event_at(reparsed, 5).End), 3725000);
	CHECK_EQ(reparsed.Undo().CurrentCommitId(), 1);
}

TEST(add_and_remove_lines) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	AssFile &file = context.Subs();
	std::size_t const original_count = file.Events.size();

	auto *added = new AssDialogue;
	added->Layer = 3;
	added->Start = agi::Time(11000);
	added->End = agi::Time(12500);
	added->Style = "Sign";
	added->Actor = "Core";
	added->Margin[0] = 10;
	added->Margin[1] = 20;
	added->Margin[2] = 30;
	added->SetText("{\\an7}Added by the core test");
	file.Events.push_back(*added);

	file.Commit("add line", AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL, -1);
	CHECK_EQ(file.Events.size(), original_count + 1);
	CHECK_EQ(context.GetLineText(static_cast<int>(original_count)), "{\\an7}Added by the core test");
	CHECK_EQ(context.Undo().UndoDescription(), "add line");

	std::string const with_added = context.SaveToText(options_lf());
	CHECK_CONTAINS(with_added, "Dialogue: 3,0:00:11.00,0:00:12.50,Sign,Core,10,20,30,,{\\an7}Added by the core test\n");

	danket::CoreContext parsed_added;
	parsed_added.LoadFromText(with_added);
	CHECK_EQ(parsed_added.Subs().Events.size(), original_count + 1);
	CHECK_EQ(parsed_added.GetLineText(static_cast<int>(original_count)), "{\\an7}Added by the core test");
	CHECK_EQ(parsed_added.GetLineText(0), "{\\fad(200,200)}It was a dark and stormy night...");

	int const removed_id = event_at(context, 1).Id;
	std::size_t removed_count = 0;
	file.Events.remove_and_dispose_if([&](AssDialogue const &line) {
		if (line.Id != removed_id)
			return false;
		++removed_count;
		return true;
	}, [](AssDialogue *line) { delete line; });
	CHECK_EQ(removed_count, std::size_t(1));
	file.Commit("remove line", AssFile::COMMIT_DIAG_ADDREM, -1);
	CHECK_EQ(file.Events.size(), original_count);

	std::string const with_removed = context.SaveToText(options_lf());
	CHECK_NOT_CONTAINS(with_removed, "Beware the");
	CHECK_CONTAINS(with_removed, "Added by the core test");
	CHECK_CONTAINS(with_removed, "It was a dark and stormy night...");

	danket::CoreContext parsed_removed;
	parsed_removed.LoadFromText(with_removed);
	CHECK_EQ(parsed_removed.Subs().Events.size(), original_count);
	CHECK_NOT_CONTAINS(parsed_removed.SaveToText(options_lf()), "Beware the");
}

// ---------------------------------------------------------------------------
// Tests: undo and redo
// ---------------------------------------------------------------------------

TEST(undo_redo_text_timing_and_lines) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	danket::UndoManager &undo = context.Undo();
	int const initial_commit = undo.CurrentCommitId();
	CHECK_EQ(initial_commit, 1);
	CHECK(!undo.CanUndo());
	CHECK(!undo.CanRedo());
	CHECK(undo.UndoDescription().empty());

	std::string const original(context.GetLineText(0));
	std::string const first_text = "{\\i1}undo me{\\i0}";
	context.SetLineText(0, first_text);
	int const text_commit = context.Subs().Commit("change text", AssFile::COMMIT_DIAG_TEXT, -1, &event_at(context, 0));
	CHECK_EQ(text_commit, initial_commit + 1);
	CHECK_EQ(undo.CurrentCommitId(), text_commit);
	CHECK(undo.CanUndo());
	CHECK(!undo.CanRedo());
	CHECK_EQ(undo.UndoDescription(), "change text");
	CHECK(undo.RedoDescription().empty());

	undo.Undo();
	CHECK_EQ(context.GetLineText(0), original);
	CHECK_EQ(undo.CurrentCommitId(), initial_commit);
	CHECK(!undo.CanUndo());
	CHECK(undo.CanRedo());
	CHECK_EQ(undo.RedoDescription(), "change text");

	undo.Redo();
	CHECK_EQ(context.GetLineText(0), first_text);
	CHECK_EQ(undo.CurrentCommitId(), text_commit);
	CHECK_EQ(undo.UndoDescription(), "change text");
	CHECK(!undo.CanRedo());

	int const old_start = int(event_at(context, 0).Start);
	int const old_end = int(event_at(context, 0).End);
	event_at(context, 0).Start = agi::Time(2000);
	event_at(context, 0).End = agi::Time(4500);
	int const timing_commit = context.Subs().Commit("change timing", AssFile::COMMIT_DIAG_TIME, -1, &event_at(context, 0));
	CHECK_EQ(timing_commit, text_commit + 1);
	CHECK_EQ(undo.UndoDescription(), "change timing");

	undo.Undo();
	CHECK_EQ(int(event_at(context, 0).Start), old_start);
	CHECK_EQ(int(event_at(context, 0).End), old_end);
	CHECK_EQ(context.GetLineText(0), first_text);
	CHECK_EQ(undo.CurrentCommitId(), text_commit);
	CHECK_EQ(undo.RedoDescription(), "change timing");

	undo.Redo();
	CHECK_EQ(int(event_at(context, 0).Start), 2000);
	CHECK_EQ(int(event_at(context, 0).End), 4500);

	std::size_t const before_add = context.Subs().Events.size();
	auto *added = new AssDialogue;
	added->Start = agi::Time(20000);
	added->End = agi::Time(21000);
	added->Style = "Default";
	added->SetText("added by undo test");
	context.Subs().Events.push_back(*added);
	context.Subs().Commit("add row", AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL, -1);
	CHECK_EQ(context.Subs().Events.size(), before_add + 1);
	CHECK_EQ(context.GetLineText(static_cast<int>(before_add)), "added by undo test");

	undo.Undo();
	CHECK_EQ(context.Subs().Events.size(), before_add);
	CHECK_EQ(undo.RedoDescription(), "add row");

	undo.Redo();
	CHECK_EQ(context.Subs().Events.size(), before_add + 1);
	CHECK_EQ(context.GetLineText(static_cast<int>(before_add)), "added by undo test");

	int const added_id = event_at(context, static_cast<int>(before_add)).Id;
	std::size_t removed_count = 0;
	context.Subs().Events.remove_and_dispose_if([&](AssDialogue const &line) {
		if (line.Id != added_id)
			return false;
		++removed_count;
		return true;
	}, [](AssDialogue *line) { delete line; });
	CHECK_EQ(removed_count, std::size_t(1));
	context.Subs().Commit("remove row", AssFile::COMMIT_DIAG_ADDREM, -1);
	CHECK_EQ(context.Subs().Events.size(), before_add);
	CHECK_EQ(undo.UndoDescription(), "remove row");

	undo.Undo();
	CHECK_EQ(context.Subs().Events.size(), before_add + 1);
	CHECK_EQ(event_at(context, static_cast<int>(before_add)).Id, added_id);
	CHECK_EQ(undo.RedoDescription(), "remove row");

	undo.Redo();
	CHECK_EQ(context.Subs().Events.size(), before_add);
	CHECK(!undo.CanRedo());
}

TEST(undo_coalescing_and_redo_clearing) {
	danket::CoreContext context;
	context.LoadFromText(sample_text());
	danket::UndoManager &undo = context.Undo();
	std::string const original(context.GetLineText(0));

	context.SetLineText(0, "typing one");
	int const first_commit = context.Subs().Commit("typing", AssFile::COMMIT_DIAG_TEXT, -1, &event_at(context, 0));
	context.SetLineText(0, "typing two");
	int const second_commit = context.Subs().Commit("typing", AssFile::COMMIT_DIAG_TEXT, first_commit, &event_at(context, 0));
	CHECK_EQ(second_commit, first_commit + 1);
	CHECK_EQ(undo.CurrentCommitId(), second_commit);
	CHECK_EQ(undo.UndoDescription(), "typing");

	undo.Undo();
	CHECK_EQ(context.GetLineText(0), original);
	CHECK(!undo.CanUndo());
	CHECK(undo.CanRedo());
	CHECK_EQ(undo.RedoDescription(), "typing");

	undo.Redo();
	CHECK_EQ(context.GetLineText(0), "typing two");
	CHECK(!undo.CanRedo());

	undo.Undo();
	CHECK(undo.CanRedo());
	CHECK_EQ(context.GetLineText(0), original);

	context.SetLineText(0, "new edit after undo");
	context.Subs().Commit("new edit", AssFile::COMMIT_DIAG_TEXT, -1, &event_at(context, 0));
	CHECK(!undo.CanRedo());
	CHECK(undo.RedoDescription().empty());
	CHECK_EQ(undo.UndoDescription(), "new edit");
	CHECK(undo.CanUndo());
}

TEST(undo_depth_and_reset) {
	danket::CoreContext context;
	context.LoadFromText(sample_text());
	danket::UndoManager &undo = context.Undo();
	CHECK_EQ(undo.CurrentCommitId(), 1);

	undo.SetMaxDepth(2);
	for (int i = 0; i < 5; ++i) {
		context.SetLineText(0, "depth edit " + std::to_string(i));
		context.Subs().Commit("depth edit " + std::to_string(i), AssFile::COMMIT_DIAG_TEXT, -1, &event_at(context, 0));
	}

	int undo_count = 0;
	while (undo.CanUndo() && undo_count < 10) {
		undo.Undo();
		++undo_count;
	}
	CHECK(undo_count >= 1);
	CHECK(undo_count <= 2);
	CHECK(undo.CanRedo());

	undo.Reset();
	CHECK(!undo.CanUndo());
	CHECK(!undo.CanRedo());
	CHECK(undo.UndoDescription().empty());
	CHECK(undo.RedoDescription().empty());
	CHECK_EQ(undo.CurrentCommitId(), 0);
}

// ---------------------------------------------------------------------------
// Tests: file I/O and writer idempotence
// ---------------------------------------------------------------------------

TEST(file_roundtrip) {
	TempDir temp;
	danket::CoreContext context;
	context.LoadFile(sample_path());

	fs::path const first_path = temp.file("roundtrip.ass");
	fs::path const second_path = temp.file("roundtrip_again.ass");
	context.SaveFile(first_path, options_lf());
	REQUIRE(fs::is_regular_file(first_path));
	std::string const first_bytes = read_file(first_path);
	CHECK(!first_bytes.starts_with("\xEF\xBB\xBF"));
	CHECK(first_bytes.starts_with("[Script Info]\nTitle: Core Test Sample\n"));

	danket::CoreContext loaded;
	loaded.LoadFile(first_path);
	CHECK_EQ(loaded.Subs().Info.size(), context.Subs().Info.size());
	CHECK_EQ(loaded.Subs().Styles.size(), context.Subs().Styles.size());
	CHECK_EQ(loaded.Subs().Events.size(), context.Subs().Events.size());
	CHECK_EQ(loaded.GetLineText(0), context.GetLineText(0));
	CHECK_EQ(loaded.GetLineText(5), context.GetLineText(5));
	expect_same_model(context.Subs(), loaded.Subs());

	loaded.SaveFile(second_path, options_lf());
	CHECK_EQ(read_file(second_path), first_bytes);
}

TEST(writer_idempotence) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	danket::AssWriteOptions const options = options_lf();

	std::string const once = context.SaveToText(options);
	danket::CoreContext second;
	second.LoadFromText(once);
	std::string const twice = second.SaveToText(options);
	CHECK_EQ(twice, once);
	expect_same_model(context.Subs(), second.Subs());

	danket::CoreContext third;
	third.LoadFromText(twice);
	CHECK_EQ(third.SaveToText(options), once);

	AssFile direct;
	danket::ReadAssText(direct, sample_text());
	std::string const direct_once = danket::WriteAssText(direct, options);
	danket::CoreContext direct_context;
	direct_context.LoadFromText(direct_once);
	CHECK_EQ(direct_context.SaveToText(options), direct_once);
}

TEST(read_ass_text_line_endings_and_bom) {
	char const *body =
		"[Script Info]\n"
		"Title: Line Endings\n"
		"ScriptType: v4.00+\n"
		"\n"
		"[V4+ Styles]\n"
		"Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
		"\n"
		"[Events]\n"
		"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,endings\n";

	std::string const lf = body;
	std::string const crlf = replace_all(lf, "\n", "\r\n");
	std::string const cr = replace_all(lf, "\n", "\r");
	std::string const bom = std::string("\xEF\xBB\xBF") + lf;

	auto verify = [](std::string const &text) {
		danket::CoreContext context;
		context.LoadFromText(text);
		CHECK_EQ(context.Subs().GetScriptInfo("Title"), "Line Endings");
		CHECK_EQ(context.Subs().Events.size(), std::size_t(1));
		CHECK_EQ(context.GetLineText(0), "endings");
	};

	verify(lf);
	verify(crlf);
	verify(cr);
	verify(bom);

	std::string const padded =
		"  [Script Info]  \r\n"
		"\tTitle:   Trimmed  \r\n"
		"ScriptType: v4.00+\r\n"
		"\r\n"
		"[Events]\r\n"
		"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,trimmed\r\n";
	danket::CoreContext context;
	context.LoadFromText(padded);
	CHECK_EQ(context.Subs().GetScriptInfo("Title"), "Trimmed");
	CHECK_EQ(context.GetLineText(0), "trimmed");
}

TEST(write_options) {
	danket::CoreContext context;
	context.LoadFile(sample_path());

	danket::AssWriteOptions options = options_lf();
	options.header_comment = "; Script generated by DanketSub tests";
	options.header_url = "; https://aegisub.org/";
	std::string const with_headers = danket::WriteAssText(context.Subs(), options);
	CHECK(with_headers.starts_with("[Script Info]\n; Script generated by DanketSub tests\n; https://aegisub.org/\nTitle: Core Test Sample\n"));

	std::string const with_defaults = context.SaveToText(options_lf());
	CHECK(with_defaults.starts_with("[Script Info]\nTitle: Core Test Sample\n"));
	CHECK_NOT_CONTAINS(with_defaults, "[Script Info]\n\n");

	options.newline = "\r\n";
	std::string const with_crlf = danket::WriteAssText(context.Subs(), options);
	CHECK(all_newlines_are_crlf(with_crlf));
	CHECK_CONTAINS(with_crlf, "Dialogue: 0,0:00:01.00,0:00:03.50,Default,Narrator,0,0,0,,{\\fad(200,200)}It was a dark and stormy night...\r\n");
	CHECK_CONTAINS(with_crlf, "\r\n[Events]\r\n");

	danket::AssWriteOptions without_properties = options_lf();
	without_properties.write_project_properties = false;
	std::string const no_properties = danket::WriteAssText(context.Subs(), without_properties);
	CHECK_NOT_CONTAINS(no_properties, "[Aegisub Project Garbage]");
	CHECK_NOT_CONTAINS(no_properties, "Audio File: audio/sample.wav");
	CHECK_NOT_CONTAINS(no_properties, "Video Zoom Percent");
	CHECK_CONTAINS(no_properties, "[V4+ Styles]");
	CHECK_CONTAINS(no_properties, "[Aegisub Extradata]");

	danket::AssWriteOptions without_ui = options_lf();
	without_ui.write_ui_state = false;
	std::string const no_ui = danket::WriteAssText(context.Subs(), without_ui);
	CHECK_CONTAINS(no_ui, "[Aegisub Project Garbage]");
	CHECK_CONTAINS(no_ui, "Audio File: audio/sample.wav");
	CHECK_CONTAINS(no_ui, "Video File: video/sample.mp4");
	CHECK_CONTAINS(no_ui, "Video AR Mode: 2");
	CHECK_CONTAINS(no_ui, "Video AR Value: 1.777778");
	CHECK_NOT_CONTAINS(no_ui, "Video Zoom Percent");
	CHECK_NOT_CONTAINS(no_ui, "Scroll Position");
	CHECK_NOT_CONTAINS(no_ui, "Active Line");
	CHECK_NOT_CONTAINS(no_ui, "Video Position");

	CHECK_CONTAINS(with_defaults, "Video Zoom Percent: 1.500000");
	CHECK_CONTAINS(with_defaults, "Scroll Position: 7");
	CHECK_CONTAINS(with_defaults, "Active Line: 3");
	CHECK_CONTAINS(with_defaults, "Video Position: 4");

	danket::AssWriteOptions without_extradata = options_lf();
	without_extradata.write_extradata = false;
	std::string const no_extradata = danket::WriteAssText(context.Subs(), without_extradata);
	CHECK_NOT_CONTAINS(no_extradata, "[Aegisub Extradata]");
	CHECK_NOT_CONTAINS(no_extradata, "Data: 0,core.test");
	CHECK_CONTAINS(no_extradata, "fontname: sample.ttf");
	CHECK_CONTAINS(no_extradata, "{\\fad(200,200)}");
}

TEST(malformed_dialogue_throws_core_exception) {
	danket::CoreContext context;

	bool caught = false;
	try {
		context.LoadFromText(
			"[Script Info]\n"
			"ScriptType: v4.00+\n"
			"\n"
			"[Events]\n"
			"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
			"Dialogue: 0,0:00:01.00,0:00:02.00,Default,,not-a-number,0,0,,text\n");
	}
	catch (agi::Exception const &) {
		caught = true;
	}

	CHECK(caught);
}

TEST(load_failure_preserves_state) {
	danket::CoreContext context;
	context.LoadFile(sample_path());
	std::string const before = context.SaveToText(options_lf());

	bool threw = false;
	try {
		context.LoadFromText("[Script Info]\nScriptType: v9.99\n");
	}
	catch (std::exception const &) {
		threw = true;
	}
	catch (...) {
		threw = true;
	}

	CHECK(threw);
	CHECK_EQ(context.SaveToText(options_lf()), before);
	CHECK(!context.Undo().CanUndo());
	CHECK(!context.Undo().CanRedo());
	CHECK_EQ(context.Undo().CurrentCommitId(), 1);
}

} // namespace

int main() {
	std::cout << std::unitbuf;
	std::size_t failed_tests = 0;
	for (auto const &test : all_tests()) {
		int const failures_before = failures;
		std::cout << "[ RUN      ] " << test.name << "\n";
		try {
			test.run();
		}
		catch (TestFailure const &) {
			// The failing check has already been reported.
		}
		catch (std::exception const &error) {
			report_uncaught(std::string("uncaught exception in ") + test.name + ": " + error.what());
		}
		catch (...) {
			report_uncaught(std::string("uncaught non-standard exception in ") + test.name);
		}

		if (failures == failures_before) {
			std::cout << "[       OK ] " << test.name << "\n";
		}
		else {
			++failed_tests;
			std::cout << "[  FAILED  ] " << test.name << "\n";
		}
	}

	std::size_t const passed = all_tests().size() - failed_tests;
	std::cout << (failed_tests == 0 ? "PASSED" : "FAILED") << ": " << passed << "/" << all_tests().size() << " tests\n";
	return failed_tests == 0 ? 0 : 1;
}
