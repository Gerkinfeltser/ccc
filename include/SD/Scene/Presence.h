#pragma once

namespace SD::Scene
{
	// Makes the player look at the person they're talking to, and turn to face
	// them. Vanilla never turns player head tracking on in third person during
	// dialogue, which only shows once the camera turns around to face the player,
	// and it never turns the player at all, so someone talked to from the side is
	// answered over a shoulder for the whole conversation.
	class Presence
	{
	public:
		// Head tracking, for one conversation.
		static void Engage(RE::Actor* a_npc);

		// The body turn, for one conversation. Separate from Engage because head
		// tracking has its own ini switch.
		static void BeginFacing(RE::Actor* a_npc);

		// Live setting ([Performance] bTurnToFace). Off mid-turn stops where it is.
		static void SetTurnToFace(bool a_on);

		// Re-applied every frame for the whole conversation. The head-track target is
		// a slot the engine and AI packages write constantly (package changes, combat
		// checks, the dialogue system's own head tracking), so a value written once
		// doesn't stay. The turn eases toward them and then holds within a margin, so
		// someone shifting their weight doesn't keep the player shuffling.
		static void Update(float a_delta);

		static void Release();

		// Goes up by one every time the player is turned. The camera hand-back compares
		// it with the value from before the conversation, to keep the view looking
		// where the player left it rather than swinging round with the body.
		[[nodiscard]] static std::uint32_t TurnSerial() noexcept;
	};
}
