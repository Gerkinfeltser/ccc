#pragma once

namespace SD::Scene
{
	// Dialogue that sounds the same however far away the camera is.
	//
	// The engine plays a line as a 3D sound that follows the speaker's head and is
	// heard from the camera. SOMDialogue3DDefault starts fading at 70 units and is
	// near silent by 3000, so a wide shot makes the voice quieter and a reverse
	// shot moves it to the other ear. While a conversation is staged, every line
	// spoken by someone in it is moved onto a node held just in front of the
	// camera, inside that 70 units, so it plays at full level from the middle of
	// the screen. The rest of the world is still heard from the camera.
	class CloseMic
	{
	public:
		static void SetEnabled(bool a_enabled) noexcept;
		[[nodiscard]] static bool Enabled() noexcept;

		// Where the camera is this frame. ApplyPose calls this with the pose it just
		// wrote; Update falls back to the camera node for frames it doesn't run.
		static void Place(const RE::NiPoint3& a_position, const RE::NiPoint3& a_forward);

		// Every staged frame: moves any new line spoken by these actors onto the
		// microphone and forgets lines that have finished.
		static void Update(std::span<RE::Actor* const> a_actors);

		// Puts every line still playing back on its speaker.
		static void Release();
	};
}
