#pragma once

#include "SD/Camera/Tripod.h"

namespace SD::Camera::WalkSource
{
	// Where the game is about to send an actor: the scene they're in first, since
	// it lists their next move before they take a step, then the AI package they
	// are running. Tripod::Predict decides what that means for the camera.
	struct Destination
	{
		bool           valid{ false };
		RE::NiPoint3   point{};
		float          radius{ 0.0f };
		Tripod::Source source{ Tripod::Source::kNone };

		// Not valid, because the package they're running keeps them where they are
		// whatever its location says (a conversation names the person spoken to).
		bool still{ false };

		// For the log.
		RE::FormID       package{ 0 };
		RE::FormID       target{ 0 };  // the reference the location resolved to, if any
		RE::FormID       scene{ 0 };
		std::uint32_t    phase{ 0 };   // the phase the scene's move starts in
		bool             later{ false };  // that phase hasn't started yet
		std::string_view how{};        // the kind of location the package named
		std::string_view kind{};       // the kind of package
	};

	[[nodiscard]] Destination Find(RE::Actor* a_actor);
}
