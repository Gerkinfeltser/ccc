#pragma once

namespace SD::Core
{
	// Camera commands from outside the game. A script writes one command per line to
	// SceneDirector_cues.txt in the SKSE log folder (Documents/My Games/Skyrim Special
	// Edition/SKSE, real on disk even under MO2, unlike Data). Each poll takes the
	// file, runs its lines and deletes it. Commands go through the same requests the
	// hotkeys use:
	//   film     start filming the conversation in front of the player (if not already)
	//   stop     stop filming it (if filming)
	//   cut      a new angle now
	//   framing  next framing: automatic, them, you, the room
	class Cues
	{
	public:
		// From the frame loop, before Director::Tick, so a request lands this frame.
		static void Poll(float a_delta);
	};
}
