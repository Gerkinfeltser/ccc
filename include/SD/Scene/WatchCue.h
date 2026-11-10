#pragma once

namespace SD::Scene
{
	// A small sign that two people nearby are talking and the camera can watch
	// them: a camera inside a ring. The ring is empty while the player is still
	// moving and fills while they stand still, and when it closes the filming
	// starts. In third person it floats over the player's head; in first person
	// it sits where the crosshair is.
	//
	// Drawn through SKSE Menu Framework, so without the framework there is no
	// sign and nothing else changes.
	class WatchCue
	{
	public:
		static void Register();

		// Every frame there is something to show, from the game's thread.
		// a_progress is 0 to 1. Left uncalled, the sign fades out on its own.
		static void Show(float a_progress);
	};
}
