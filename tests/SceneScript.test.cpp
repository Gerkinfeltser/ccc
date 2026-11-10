#include "SD/Dialogue/SceneScript.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
	void Expect(bool condition, const char* message)
	{
		if (!condition) {
			std::cerr << "SceneScript: " << message << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	using SD::Dialogue::Ahead;
	using SD::Dialogue::FirstMove;
	using SD::Dialogue::Look;
	using SD::Dialogue::LookAheadEnd;
	using SD::Dialogue::NextSpokenPhase;
	using SD::Dialogue::ScriptLine;
	using SD::Dialogue::ScriptMove;

	// MQ101DragonAttackScene1 (000D0594), the Helgen tower, as Skyrim.esm has it:
	// every dialogue action with a topic, by phase span and whether it loops.
	// Ralof's "Up through the tower, let's go!" is phase 10; 11 and 12 are a run
	// and a timer; 13 is Ulfric, Ralof and a soldier repeating until the player
	// reaches the tower.
	constexpr std::array<ScriptLine, 15> kTower{ {
		{ 1, 1, false },    // Ralof
		{ 2, 2, false },    // Ralof
		{ 9, 9, false },    // Ulfric: "We need to move. Now!"
		{ 13, 13, true },   // Ulfric: "Move! Up the tower!"
		{ 20, 20, false },  // Ralof
		{ 21, 21, false },  // Ralof
		{ 16, 17, false },  // Ralof
		{ 25, 25, true },   // Ulfric
		{ 25, 25, true },   // Ralof
		{ 3, 3, true },     // Ralof
		{ 6, 6, false },    // Ralof: "Jarl Ulfric! What is that thing?"
		{ 10, 10, false },  // Ralof: "Up through the tower, let's go!"
		{ 7, 7, false },    // Ulfric: "Legends don't burn down villages."
		{ 13, 13, true },   // Ralof: "With me, up the tower!"
		{ 13, 13, true },   // Stormcloak soldier: "We'll have to carry this one..."
	} };
	constexpr std::uint32_t kTowerPhases = 26;
}

int main()
{
	// The tower, phase by phase.
	Expect(Look(kTower, 6, kTowerPhases) == Ahead::kScripted, "the exchange with Ulfric is story");
	Expect(Look(kTower, 9, kTowerPhases) == Ahead::kScripted, "Ulfric's order is story");
	Expect(Look(kTower, 10, kTowerPhases) == Ahead::kScripted, "Ralof's line is still story while it plays");
	Expect(Look(kTower, 11, kTowerPhases) == Ahead::kMarkingTime,
		"after Ralof's line, the run and the timer lead only into repeats");
	Expect(Look(kTower, 12, kTowerPhases) == Ahead::kMarkingTime, "the timer phase looks ahead to the repeats");
	Expect(Look(kTower, 13, kTowerPhases) == Ahead::kMarkingTime, "the repeats are marking time");
	Expect(NextSpokenPhase(kTower, 11, kTowerPhases) == 13, "the run and the timer are skipped over");
	Expect(Look(kTower, 14, kTowerPhases) == Ahead::kScripted, "inside the tower, the story resumes");
	Expect(Look(kTower, 22, kTowerPhases) == Ahead::kMarkingTime, "the scene ends on repeats");
	Expect(Look(kTower, 26, kTowerPhases) == Ahead::kNothing, "past the last phase");

	// A loop running alongside a line that plays once is still story: Tullius
	// shouting through phases 2 to 9 of the keep scene does not make Hadvar's
	// line in phase 8 filler.
	constexpr std::array<ScriptLine, 3> kKeep{ {
		{ 2, 9, true },
		{ 8, 8, false },
		{ 9, 9, true },
	} };
	Expect(Look(kKeep, 8, 10) == Ahead::kScripted, "a once-only line beside a loop is story");
	Expect(Look(kKeep, 9, 10) == Ahead::kMarkingTime, "only loops left");
	Expect(Look(kKeep, 2, 10) == Ahead::kMarkingTime, "a phase with only the loop in it marks time");

	// Nothing left to say: the last line was in phase 3 and the scene walks on.
	constexpr std::array<ScriptLine, 2> kWalkOff{ {
		{ 0, 0, false },
		{ 3, 3, false },
	} };
	Expect(Look(kWalkOff, 3, 6) == Ahead::kScripted, "the last line itself");
	Expect(Look(kWalkOff, 4, 6) == Ahead::kNothing, "the walk after it");

	// A span that ends before it starts is ignored rather than trusted.
	constexpr std::array<ScriptLine, 2> kBroken{ {
		{ 5, 2, false },
		{ 4, 4, true },
	} };
	Expect(Look(kBroken, 0, 6) == Ahead::kMarkingTime, "a reversed span says nothing");

	// No lines at all.
	Expect(Look(std::span<const ScriptLine>{}, 0, 4) == Ahead::kNothing, "a scene of movement only");

	// Looking ahead for a walk. Ralof's line is phase 10; the run is 11 and the
	// timer 12, and the repeats start at 13, so a cut on his line can see the run.
	Expect(LookAheadEnd(kTower, 10, kTowerPhases) == 12, "the run and the timer are before the next line");
	Expect(LookAheadEnd(kTower, 1, kTowerPhases) == 1, "a line straight after another leaves nothing between");
	Expect(LookAheadEnd(kTower, 25, kTowerPhases) == 25, "the last line looks no further than its own phase");
	Expect(LookAheadEnd(kWalkOff, 3, 6) == 5, "after the last line, the rest of the scene");
	Expect(LookAheadEnd(std::span<const ScriptLine>{}, 2, 0) == 2, "an empty scene");

	constexpr std::array<ScriptMove, 3> kMoves{ {
		{ 11, 11 },  // Ralof runs for the tower
		{ 14, 15 },
		{ 4, 2 },    // reversed, ignored
	} };
	Expect(FirstMove(kMoves, 10, LookAheadEnd(kTower, 10, kTowerPhases)) == 0, "the run is found from the line");
	Expect(FirstMove(kMoves, 11, 12) == 0, "the run while it plays");
	Expect(FirstMove(kMoves, 12, 12) == -1, "nothing once the run is over");
	Expect(FirstMove(kMoves, 13, 20) == 1, "the next walk inside the tower");
	Expect(FirstMove(kMoves, 0, 3) == -1, "a reversed span says nothing");

	constexpr std::array<ScriptMove, 2> kOverlap{ {
		{ 8, 12 },
		{ 6, 9 },
	} };
	Expect(FirstMove(kOverlap, 7, 10) == 1, "the move already running wins over one starting later");
	Expect(FirstMove(kOverlap, 10, 10) == 0, "only the one still running");

	std::cout << "SceneScript: all tests passed\n";
	return EXIT_SUCCESS;
}
