#pragma once

namespace SD::Dialogue
{
	// Other people's lines when the game keeps no subtitles. SceneWatch reads NPC
	// lines from the engine's subtitle list, but the engine adds nothing to that
	// list while General Subtitles and Dialogue Subtitles are both off, so with
	// both off nobody else is heard. This sits on the engine's add and keeps the
	// lines it turns away. With either setting on it keeps nothing, and the list
	// stays the only source.
	class SubtitleTap
	{
	public:
		struct Caught
		{
			RE::FormID                            speaker{ 0 };
			std::string                           text{};
			std::chrono::steady_clock::time_point at{};
		};

		// Finds the engine's add (by Address Library ID on 1.5.97, otherwise by the
		// settings test inside it) and hooks its entry. Logs and leaves the game alone
		// if it can't be found or its first bytes can't be moved.
		static void Install();

		[[nodiscard]] static bool Installed() noexcept;

		// General and Dialogue Subtitles are both off right now. False if the settings
		// couldn't be found.
		[[nodiscard]] static bool SubtitlesOff() noexcept;

		// Lines turned away since the last call, oldest first. Any thread may add
		// them; the main thread takes them.
		[[nodiscard]] static std::vector<Caught> Take();
	};
}
