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
	//   film <speaker> <other>   film these two (form IDs, e.g. film 0x1A66E 0x813BA)
	//   line <speaker> <words>   a line of a script starts now (form ID, word count); from the
	//                            first, the scene follows these, not subtitles, until stopped
	//   shot <name>              cut to that shot, named as the log prints it: shot "Close Up"
	//                            (the side, them or you, follows who the frame is on)
	class Cues
	{
	public:
		// From the frame loop, before Director::Tick, so a request lands this frame.
		static void Poll(float a_delta);
	};
}
