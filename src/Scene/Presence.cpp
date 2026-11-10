#include "SD/Scene/Presence.h"

#include "SD/Core/Logging.h"

#include <numbers>

namespace SD::Scene
{
	namespace
	{
		bool            engaged{ false };
		bool            restoreHeadTracking{ false };
		RE::ActorHandle partner{};
		float           graphCountdown{ 0.0f };
		Log::OnceFlag   reported;

		// The target is set every frame (a cheap pointer write); the graph variable
		// only a few times a second, since it goes through the behaviour graph.
		constexpr float kGraphInterval = 0.2f;

		[[nodiscard]] RE::HighProcessData* HighOf(RE::Actor* a_actor)
		{
			auto* process = a_actor ? a_actor->GetActorRuntimeData().currentProcess : nullptr;
			return process ? process->high : nullptr;
		}

		// ---- Turning to face them -------------------------------------------------

		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr float kDegree = kPi / 180.0f;

		// Off by more than this when the conversation starts, the player turns. Small
		// enough that talking to someone from the side always turns; anything under it
		// is already facing them as far as anyone can tell.
		constexpr float kStartOff = 10.0f * kDegree;

		// Once facing them, how far they have to drift before the player turns again.
		constexpr float kRefaceOff = 30.0f * kDegree;

		// Turn speed, which sets the duration: a quarter turn takes about 0.4 s. The
		// clamp keeps a small correction from snapping and a half turn from dragging.
		constexpr float kTurnSpeed = 230.0f * kDegree;
		constexpr float kMinTurnSeconds = 0.25f;
		constexpr float kMaxTurnSeconds = 0.85f;

		// Closer than this on the floor there's no meaningful direction to face.
		constexpr float kMinFacingDistance = 20.0f;

		// A heading moved by something else between two of our writes. A few in a row
		// means another mod is steering the player, and the turn stops for this
		// conversation rather than fight it.
		constexpr float kContestedDrift = 1.5f * kDegree;
		constexpr int   kContestedFrames = 4;

		bool            turnToFace{ true };
		bool            facingEngaged{ false };
		RE::ActorHandle facing{};

		struct Turn
		{
			bool  active{ false };
			float from{ 0.0f };
			float elapsed{ 0.0f };
			float duration{ 0.0f };
			float written{ 0.0f };
			int   contested{ 0 };
		};
		Turn          turn{};
		bool          faced{ false };     // finished at least one turn this conversation
		bool          gaveUp{ false };    // another mod steers the player
		bool          blockedLogged{ false };
		std::uint32_t turnSerial{ 0 };

		[[nodiscard]] float Wrap(float a_radians)
		{
			a_radians = std::fmod(a_radians + kPi, 2.0f * kPi);
			if (a_radians < 0.0f) {
				a_radians += 2.0f * kPi;
			}
			return a_radians - kPi;
		}

		[[nodiscard]] float Heading0To2Pi(float a_radians)
		{
			a_radians = std::fmod(a_radians, 2.0f * kPi);
			return a_radians < 0.0f ? a_radians + 2.0f * kPi : a_radians;
		}

		[[nodiscard]] float Smooth(float a_t)
		{
			a_t = std::clamp(a_t, 0.0f, 1.0f);
			return a_t * a_t * (3.0f - 2.0f * a_t);
		}

		[[nodiscard]] const char* NameOf(RE::Actor* a_actor)
		{
			const char* name = a_actor ? a_actor->GetName() : nullptr;
			return (name && *name) ? name : "<unnamed>";
		}

		// Why the player can't be turned right now, or empty if they can. Anything
		// that already decides where their body points (a seat, a mount, a killmove,
		// a stagger, a fall) is left alone.
		[[nodiscard]] std::string_view CannotTurn(RE::PlayerCharacter* a_player)
		{
			const auto* state = a_player->AsActorState();
			if (state && state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
				return "seated or in furniture"sv;
			}
			if (a_player->GetOccupiedFurniture()) {
				return "using furniture"sv;
			}
			if (a_player->IsOnMount()) {
				return "mounted"sv;
			}
			if (a_player->IsInKillMove()) {
				return "in a killmove"sv;
			}
			if (state && state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal) {
				return "knocked down"sv;
			}
			if (a_player->IsInMidair()) {
				return "in the air"sv;
			}
			return {};
		}

		void UpdateFacing(RE::PlayerCharacter* a_player, RE::Actor* a_npc, float a_delta)
		{
			if (!turnToFace || gaveUp) {
				turn.active = false;
				return;
			}

			if (const auto why = CannotTurn(a_player); !why.empty()) {
				if (turn.active || !blockedLogged) {
					Log::Info(Log::Category::kStaging, "Not turning you to face {}: {}."sv, NameOf(a_npc), why);
					blockedLogged = true;
				}
				turn.active = false;
				return;
			}

			const auto  from = a_player->GetPosition();
			const auto  to = a_npc->GetPosition();
			const float dx = to.x - from.x;
			const float dy = to.y - from.y;
			if (dx * dx + dy * dy < kMinFacingDistance * kMinFacingDistance) {
				return;
			}

			// Skyrim's heading is 0 along +Y and grows clockwise, so atan2 takes x first.
			const float target = std::atan2(dx, dy);
			const float current = a_player->data.angle.z;

			if (turn.active) {
				// Something else moved the player between our writes.
				if (std::abs(Wrap(current - turn.written)) > kContestedDrift) {
					if (++turn.contested >= kContestedFrames) {
						gaveUp = true;
						turn.active = false;
						Log::Info(Log::Category::kStaging,
							"Another mod is turning you during this conversation; leaving your heading to it."sv);
						return;
					}
				} else {
					turn.contested = 0;
				}
			}

			const float off = Wrap(target - current);
			if (!turn.active) {
				if (std::abs(off) < (faced ? kRefaceOff : kStartOff)) {
					return;
				}
				turn.active = true;
				turn.from = current;
				turn.elapsed = 0.0f;
				turn.duration = std::clamp(std::abs(off) / kTurnSpeed, kMinTurnSeconds, kMaxTurnSeconds);
				turn.contested = 0;
				++turnSerial;
				Log::Info(Log::Category::kStaging, "Turning you to face {} ({:.0f} degrees over {:.2f}s)."sv,
					NameOf(a_npc), off / kDegree, turn.duration);
			}

			// Eased from where the turn started toward where they are now, so someone
			// still stepping into place is followed.
			turn.elapsed += std::clamp(a_delta, 0.0f, 0.1f);
			const float t = turn.duration > 0.0f ? turn.elapsed / turn.duration : 1.0f;
			const float heading = Heading0To2Pi(turn.from + Wrap(target - turn.from) * Smooth(t));
			a_player->SetHeading(heading);
			turn.written = heading;

			if (t >= 1.0f) {
				turn.active = false;
				faced = true;
			}
		}
	}

	void Presence::Engage(RE::Actor* a_npc)
	{
		if (engaged || !a_npc) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Two things are needed: the graph variable allows the head rotation at all,
		// and the target says where to look. The target alone does nothing.
		bool previous = false;
		if (player->GetGraphVariableBool("bHeadTracking", previous)) {
			restoreHeadTracking = !previous;
		}
		player->SetGraphVariableBool("bHeadTracking", true);

		if (auto* high = HighOf(player)) {
			high->SetHeadtrackTarget(RE::HighProcessData::HEAD_TRACK_TYPES::kDialogue, a_npc);
		}

		// The NPC looks back. Usually they already do, but a forcegreet or scene line
		// can leave them facing where they were walking.
		if (auto* high = HighOf(a_npc)) {
			high->SetHeadtrackTarget(RE::HighProcessData::HEAD_TRACK_TYPES::kDialogue, player);
		}

		engaged = true;
		partner = a_npc->GetHandle();
		graphCountdown = kGraphInterval;

		if (reported.Take()) {
			Log::Info(Log::Category::kStaging,
				"Headtracking engaged; player graph variable was {}."sv,
				restoreHeadTracking ? "off"sv : "already on"sv);
		}
	}

	void Presence::BeginFacing(RE::Actor* a_npc)
	{
		if (!a_npc) {
			return;
		}
		// Re-staged for the same person: carry on with the turn already under way.
		if (facingEngaged && facing.get().get() == a_npc) {
			return;
		}
		facingEngaged = true;
		facing = a_npc->GetHandle();
		turn = {};
		faced = false;
		gaveUp = false;
		blockedLogged = false;
	}

	void Presence::SetTurnToFace(bool a_on)
	{
		turnToFace = a_on;
		if (!a_on) {
			turn.active = false;
		}
	}

	void Presence::Update(float a_delta)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		if (facingEngaged) {
			if (auto npc = facing.get()) {
				UpdateFacing(player, npc.get(), a_delta);
			}
		}

		if (!engaged) {
			return;
		}

		auto npc = partner.get();
		if (!npc) {
			return;
		}

		if (auto* high = HighOf(player)) {
			high->SetHeadtrackTarget(RE::HighProcessData::HEAD_TRACK_TYPES::kDialogue, npc.get());
		}
		if (auto* high = HighOf(npc.get())) {
			high->SetHeadtrackTarget(RE::HighProcessData::HEAD_TRACK_TYPES::kDialogue, player);
		}

		graphCountdown -= a_delta;
		if (graphCountdown <= 0.0f) {
			graphCountdown = kGraphInterval;

			// The permission half. Other mods switch it off (combat behaviour, mount
			// transitions, their own head tracking), and with it false the target does
			// nothing.
			player->SetGraphVariableBool("bHeadTracking", true);
		}
	}

	void Presence::Release()
	{
		// A turn in progress just stops; the player keeps the heading it reached.
		facingEngaged = false;
		facing = {};
		turn.active = false;

		if (!engaged) {
			return;
		}
		engaged = false;
		partner = {};

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		if (auto* high = HighOf(player)) {
			high->SetHeadtrackTarget(RE::HighProcessData::HEAD_TRACK_TYPES::kDialogue, nullptr);
		}

		// Only clear the graph variable if this turned it on; other mods enable player
		// head tracking too.
		if (restoreHeadTracking) {
			player->SetGraphVariableBool("bHeadTracking", false);
			restoreHeadTracking = false;
		}
	}

	std::uint32_t Presence::TurnSerial() noexcept
	{
		return turnSerial;
	}
}
