#include "SD/Core/Cues.h"

#include "SD/Camera/Director.h"
#include "SD/Core/Logging.h"

#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

namespace SD::Core
{
	namespace
	{
		// ponytail: polled file, about 50 ms late at worst; a named pipe if cues ever need frame accuracy.
		constexpr float kPollSeconds = 0.05f;

		void Run(std::string a_line)
		{
			std::erase_if(a_line, [](char c) { return c == '\r'; });
			const auto first = a_line.find_first_not_of(" \t");
			if (first == std::string::npos || a_line[first] == '#') {
				return;
			}
			auto cmd = a_line.substr(first, a_line.find_last_not_of(" \t") - first + 1);
			std::ranges::transform(cmd, cmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			// The rest of the line after the command word, quotes dropped.
			std::string args;
			if (const auto space = cmd.find_first_of(" \t"); space != std::string::npos) {
				args = cmd.substr(cmd.find_first_not_of(" \t", space));
				cmd.resize(space);
				std::erase(args, '"');
			}

			// The first two argument words, and a word as a number (0 if it isn't one).
			std::string word1, word2;
			std::istringstream(args) >> word1 >> word2;
			const auto number = [](std::string_view a_word, int a_base) -> std::uint32_t {
				if (a_base == 16 && a_word.starts_with("0x")) {
					a_word.remove_prefix(2);
				}
				std::uint32_t value = 0;
				const auto [end, ec] = std::from_chars(a_word.data(), a_word.data() + a_word.size(), value, a_base);
				return ec == std::errc{} && end == a_word.data() + a_word.size() ? value : 0;
			};

			using Camera::Director;
			if (cmd == "line") {
				// line <speaker> <words>
				const auto who = number(word1, 16);
				if (!who) {
					Log::Warn(Log::Category::kCore, "Cue line: '{}' doesn't start with a form ID (line 0x1A66E 12)"sv, args);
					return;
				}
				Director::RequestLine(who, number(word2, 10));
			} else if (cmd == "film" && !args.empty()) {
				// film <speaker> <other>, as form IDs (0x1A66E or 1A66E).
				const auto a = number(word1, 16);
				const auto b = number(word2, 16);
				if (!a || !b) {
					Log::Warn(Log::Category::kCore, "Cue film: '{}' isn't two form IDs (film 0x1A66E 0x813BA)"sv, args);
					return;
				}
				Director::RequestFilm(a, b);
			} else if (cmd == "shot") {
				if (!Director::Staging()) {
					Log::Info(Log::Category::kCore, "Cue shot: nothing being filmed, dropped"sv);
					return;
				}
				if (!Director::RequestShot(args)) {
					Log::Warn(Log::Category::kCore, "Cue shot: no shot is called '{}' (names as the log's Cut lines print them)"sv, args);
					return;
				}
			} else if (!args.empty()) {
				Log::Warn(Log::Category::kCore, "Cue not understood: '{} {}'"sv, cmd, args);
				return;
			} else if (cmd == "film") {
				if (Director::FilmingScene()) {
					Log::Info(Log::Category::kCore, "Cue film: already filming"sv);
					return;
				}
				Director::RequestScene();
			} else if (cmd == "stop") {
				if (!Director::FilmingScene()) {
					Log::Info(Log::Category::kCore, "Cue stop: not filming"sv);
					return;
				}
				Director::RequestScene();
			} else if (cmd == "cut" || cmd == "framing") {
				if (!Director::Staging()) {
					Log::Info(Log::Category::kCore, "Cue {}: nothing being filmed, dropped"sv, cmd);
					return;
				}
				cmd == "cut" ? Director::RequestCut() : Director::RequestFraming();
			} else {
				Log::Warn(Log::Category::kCore, "Cue not understood: '{}' (film, stop, cut, framing, shot, line)"sv, cmd);
				return;
			}
			Log::Info(Log::Category::kCore, "Cue {}{}{}"sv, cmd, args.empty() ? "" : " ", args);
		}
	}

	void Cues::Poll(float a_delta)
	{
		static float                 wait = 0.0f;
		static std::filesystem::path file{};
		static std::filesystem::path taken{};

		if ((wait -= a_delta) > 0.0f) {
			return;
		}
		wait = kPollSeconds;

		if (file.empty()) {
			const auto dir = logger::log_directory();
			if (!dir) {
				wait = 1.0e9f;  // no log folder, no cues
				return;
			}
			file = *dir / "SceneDirector_cues.txt";
			taken = *dir / "SceneDirector_cues.taking";
		}

		std::error_code ec;
		if (!std::filesystem::exists(file, ec)) {
			return;
		}
		// Rename first: it fails while the writer still has the file open, so a
		// half-written file is retried next poll instead of read.
		std::filesystem::rename(file, taken, ec);
		if (ec) {
			return;
		}

		std::ifstream in(taken);
		for (std::string line; std::getline(in, line);) {
			Run(std::move(line));
		}
		in.close();
		std::filesystem::remove(taken, ec);
	}
}
