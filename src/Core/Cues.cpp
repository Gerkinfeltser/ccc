#include "SD/Core/Cues.h"

#include "SD/Camera/Director.h"
#include "SD/Core/Logging.h"

#include <cctype>
#include <fstream>

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

			using Camera::Director;
			if (cmd == "film") {
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
				Log::Warn(Log::Category::kCore, "Cue not understood: '{}' (film, stop, cut, framing)"sv, cmd);
				return;
			}
			Log::Info(Log::Category::kCore, "Cue {}"sv, cmd);
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
