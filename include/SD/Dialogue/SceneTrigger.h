#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace SD::Dialogue
{
	// When to start filming other people's conversations without being asked. The
	// key always counts as intent; auto mode reads it from the player standing
	// still in range, looking at an exchange that's actually happening, and stops
	// as soon as they move.
	//
	// Pure, so the timing can be tested. SceneWatch decides what counts as a
	// candidate and the Director stages it.
	class SceneTrigger
	{
	public:
		struct Settings
		{
			bool  enabled{ false };
			float waitSeconds{ 1.5f };  // standing still this long before filming

			// Off: the wait is time spent standing still, so someone already standing
			// there is filmed on the first line. On: it runs from the moment there is
			// something to watch, so the sign that shows the wait is always seen
			// counting.
			bool fromSighting{ false };
		};

		struct Input
		{
			// Two NPCs have exchanged lines recently, in range and in view.
			bool candidate{ false };

			// Which pair it is, so walking away from one scene doesn't block the next.
			std::uint64_t key{ 0 };

			// No movement input this frame.
			bool still{ false };

			// Anything that rules filming out: combat, a menu, the player's own
			// conversation, a drawn weapon, a mount, the camera already staged.
			bool blocked{ false };

			float delta{ 0.0f };
		};

		// True on the frame filming should start.
		[[nodiscard]] bool Update(const Settings& a_settings, const Input& a_input)
		{
			const float delta = std::max(a_input.delta, 0.0f);
			snoozeLeft = std::max(snoozeLeft - delta, 0.0f);
			progress = -1.0f;

			if (!a_settings.enabled || a_input.blocked) {
				stillFor = 0.0f;
				watchedFor = 0.0f;
				return false;
			}

			// A scene the player just walked out of stays out until they've left it behind
			// or some time has passed; otherwise stopping to look back would film it
			// again.
			const bool watchable = a_input.candidate && !(snoozeLeft > 0.0f && a_input.key == snoozedKey);

			if (!a_input.still) {
				stillFor = 0.0f;
				watchedFor = 0.0f;
				if (watchable) {
					progress = 0.0f;  // something to watch, if they stop
				}
				return false;
			}

			stillFor += delta;

			if (!watchable) {
				// A candidate comes and goes with the gaps between lines. The wait
				// only starts over once it has really gone.
				lostFor += delta;
				if (lostFor > kLostSeconds) {
					watchedFor = 0.0f;
				}
				return false;
			}
			lostFor = 0.0f;

			watchedFor += delta;
			const float waited = a_settings.fromSighting ? watchedFor : stillFor;
			const float wait = std::max(a_settings.waitSeconds, 0.0f);
			if (waited < wait) {
				progress = waited / wait;
				return false;
			}

			progress = 1.0f;
			stillFor = 0.0f;
			watchedFor = 0.0f;
			return true;
		}

		// How far through the wait the last Update was: 0 when there is something
		// to watch and the player is still moving, up to 1 on the frame filming
		// starts. Negative when there is nothing to show.
		[[nodiscard]] float Progress() const { return progress; }

		// The player ended filming by moving or with the key. That pair stays out of
		// auto mode for a while; other pairs are unaffected.
		void Snooze(std::uint64_t a_key, float a_seconds = kSnoozeSeconds)
		{
			snoozedKey = a_key;
			snoozeLeft = a_seconds;
			stillFor = 0.0f;
			watchedFor = 0.0f;
			progress = -1.0f;
		}

		void Reset()
		{
			stillFor = 0.0f;
			watchedFor = 0.0f;
			progress = -1.0f;
			snoozeLeft = 0.0f;
			snoozedKey = 0;
		}

		// Filming just stopped, however it started. The player has to stand still
		// again before auto mode takes the camera; time already spent standing still
		// doesn't count as a new decision.
		void Restart()
		{
			stillFor = 0.0f;
			watchedFor = 0.0f;
			progress = -1.0f;
		}

		[[nodiscard]] float StillFor() const { return stillFor; }

		static constexpr float kSnoozeSeconds = 25.0f;
		static constexpr float kLostSeconds = 0.5f;

	private:
		float         stillFor{ 0.0f };
		float         watchedFor{ 0.0f };
		float         lostFor{ 0.0f };
		float         progress{ -1.0f };
		float         snoozeLeft{ 0.0f };
		std::uint64_t snoozedKey{ 0 };
	};

	// An order-independent key for a pair of actors.
	[[nodiscard]] constexpr std::uint64_t PairKey(std::uint32_t a_one, std::uint32_t a_two) noexcept
	{
		const auto low = std::min(a_one, a_two);
		const auto high = std::max(a_one, a_two);
		return (static_cast<std::uint64_t>(high) << 32) | low;
	}

	// One recorded line, as the back-and-forth test sees it.
	struct HeardTurn
	{
		std::uint32_t speaker{ 0 };
		float         age{ 0.0f };  // seconds since the line started
		bool          toPlayer{ false };
	};

	// Is this a conversation, or two people who happened to speak? Counts how many
	// times the floor changed hands between the two, newest line first, through
	// their latest unbroken run. A gap longer than a_maxGap ends the run, as does
	// a line either of them said to the player; a third voice neither breaks nor
	// counts. Two changes (A, B, A) is a back-and-forth.
	[[nodiscard]] inline int Exchanges(std::span<const HeardTurn> a_newestFirst, std::uint32_t a_one,
		std::uint32_t a_two, float a_maxGap)
	{
		int           changes = 0;
		std::uint32_t last = 0;
		float         lastAge = 0.0f;
		bool          any = false;

		for (const auto& turn : a_newestFirst) {
			if (turn.speaker != a_one && turn.speaker != a_two) {
				continue;
			}
			if (turn.toPlayer) {
				break;
			}
			if (any) {
				if (turn.age - lastAge > a_maxGap) {
					break;
				}
				if (turn.speaker != last) {
					++changes;
				}
			}
			last = turn.speaker;
			lastAge = turn.age;
			any = true;
		}
		return changes;
	}

	// Start to start: a few seconds of line and a breath before the reply.
	constexpr float kReplyGapSeconds = 10.0f;

	// Why two NPCs' lines count as a conversation, for auto mode.
	enum class StrictPairing : std::uint8_t
	{
		kNone,
		kOneScene,        // both in one scene the game is playing
		kOneSceneForYou,  // the same, with you the one being addressed
		kBackAndForth,    // no scene, but the floor changed hands twice
	};

	// Two voices in the same game scene count, whoever they're addressing; that
	// covers scenes staged with the player as the audience, where most lines are
	// said to the player. Outside a scene, a line said to the player never counts,
	// which keeps out greetings in passing and followers' asides.
	[[nodiscard]] constexpr StrictPairing StrictPair(bool a_sameScene, bool a_lineToYou, bool a_replyToYou,
		int a_exchanges) noexcept
	{
		if (a_sameScene) {
			return (a_lineToYou || a_replyToYou) ? StrictPairing::kOneSceneForYou : StrictPairing::kOneScene;
		}
		if (!a_lineToYou && !a_replyToYou && a_exchanges >= 2) {
			return StrictPairing::kBackAndForth;
		}
		return StrictPairing::kNone;
	}
}
