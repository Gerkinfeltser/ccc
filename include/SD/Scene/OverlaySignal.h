#pragma once

namespace SD::Scene
{
	// Interface other mods draw themselves, with ImGui at Present rather than as a
	// game menu, is out of Interface's reach. Several of those mods hide while a
	// game menu is open, which covers your own conversations, but nothing is open
	// while other people talk. They also hide while the global PhotoMode_IsActive
	// (Photo Mode's) is above 0.5 (Custom Markers' mini map, Skyrim Party Sheet), so
	// it's raised while other people's conversations are filmed and put back
	// after.
	class OverlaySignal
	{
	public:
		// At data load: finds the global, or makes one in memory when Photo Mode isn't
		// installed. Nothing is written to a plugin or the save.
		static void Prepare();

		// Every frame while filming other people's conversations; puts the value back
		// if anything lowered it.
		static void Raise();

		// Back to the value it had before Raise. Safe to call when not raised.
		static void Lower();

		// Before a save: the save keeps the value from before Raise. The next Raise
		// puts it up again.
		static void BeforeSave();
	};
}
