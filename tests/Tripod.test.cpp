#include "SD/Camera/Tripod.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
	void Expect(bool condition, const char* message)
	{
		if (!condition) {
			std::cerr << "Tripod: " << message << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	bool Near(float a, float b, float tolerance = 0.5f)
	{
		return std::abs(a - b) <= tolerance;
	}

	using namespace SD::Camera::Tripod;

	Evidence Heading(Point a_destination, float a_radius, Source a_source)
	{
		Evidence evidence{};
		evidence.hasDestination = true;
		evidence.destination = a_destination;
		evidence.arriveRadius = a_radius;
		evidence.destinationSource = a_source;
		return evidence;
	}
}

int main()
{
	// ---- Forecasts ----------------------------------------------------------

	{
		// A long walk is forecast for the length of a shot at a walking pace.
		const auto f = Predict(Heading({ 1000.0f, 0.0f, 0.0f }, 50.0f, Source::kScenePackage));
		Expect(f.moving, "a far destination is a walk");
		Expect(f.source == Source::kScenePackage, "the scene said so");
		Expect(Near(f.distance, kWalkSpeed * kHorizonSeconds), "a shot's worth of walking");
		Expect(Near(f.to.x, kWalkSpeed * kHorizonSeconds) && Near(f.to.y, 0.0f), "toward the destination");
		Expect(Near(f.remaining, 950.0f), "the rest of the way, to the edge of the radius");
		Expect(!f.reaches, "not there by the end of the shot");
	}
	{
		// A short walk ends at the destination.
		const auto f = Predict(Heading({ 0.0f, 300.0f, 20.0f }, 0.0f, Source::kPackage));
		Expect(f.moving && f.reaches, "a short walk gets there");
		Expect(Near(f.to.y, 300.0f) && Near(f.to.z, 20.0f), "and ends on it");
	}
	{
		// Someone already running is forecast at their own pace.
		auto evidence = Heading({ 4000.0f, 0.0f, 0.0f }, 0.0f, Source::kScenePackage);
		evidence.velocity = { 300.0f, 0.0f, 0.0f };
		const auto f = Predict(evidence);
		Expect(Near(f.distance, 300.0f * kHorizonSeconds), "a run covers more ground");
	}
	{
		// Already inside the scene package's radius: staying, whatever their feet do.
		auto evidence = Heading({ 200.0f, 0.0f, 0.0f }, 256.0f, Source::kScenePackage);
		evidence.velocity = { 0.0f, 100.0f, 0.0f };
		Expect(!Predict(evidence).moving, "the scene says they've arrived");

		// An AI package that's satisfied says nothing; the velocity decides.
		evidence.destinationSource = Source::kPackage;
		const auto f = Predict(evidence);
		Expect(f.moving && f.source == Source::kVelocity, "a package they're already at defers to how they move");
	}
	{
		// A destination closer than a walk worth re-framing for.
		Expect(!Predict(Heading({ 100.0f, 0.0f, 0.0f }, 0.0f, Source::kScenePackage)).moving, "a step isn't a walk");
	}
	{
		// No destination: only a real walk counts.
		Evidence evidence{};
		evidence.velocity = { 0.0f, 50.0f, 0.0f };
		const auto f = Predict(evidence);
		Expect(f.moving && f.source == Source::kVelocity, "walking with nowhere known to go");
		Expect(Near(f.to.y, 50.0f * kVelocityHorizonSeconds), "extrapolated a few seconds");

		evidence.velocity = { 35.0f, 0.0f, 0.0f };
		Expect(!Predict(evidence).moving, "an amble that won't get anywhere");
		evidence.velocity = { 20.0f, 0.0f, 0.0f };
		Expect(!Predict(evidence).moving, "a weight shift");
		Expect(!Predict(Evidence{}).moving, "standing");
	}


	// ---- Where things land in a frame -----------------------------------------

	constexpr Lens   wide60{ 60.0f, 1.78f, 0.0f, 0.0f };
	constexpr Limits limits{};
	const Point      camera300{ 0.0f, -300.0f, 0.0f };
	const Point      origin{ 0.0f, 0.0f, 0.0f };
	{
		const std::array centred{ Sample{ origin, 10.0f, 10.0f, 10.0f } };
		const auto       c = Cover(camera300, origin, wide60, centred);
		Expect(!c.behind && Near(c.nearest, 300.0f), "a head straight ahead is 300 deep");
		Expect(Near(c.worst, 10.0f / 300.0f / (0.57735f / 1.78f), 0.01f), "the tall edge decides a small box");

		const std::array aside{ Sample{ { 200.0f, 0.0f, 0.0f }, 10.0f, 10.0f, 10.0f } };
		Expect(Cover(camera300, origin, wide60, aside).worst > 1.0f, "200 to the side at 300 is outside a 60 degree lens");

		const std::array behind{ Sample{ { 0.0f, -400.0f, 0.0f }, 10.0f, 10.0f, 10.0f } };
		Expect(Cover(camera300, origin, wide60, behind).behind, "behind the camera");
	}
	{
		// The black bars take height off the frame.
		const std::array high{ Sample{ { 0.0f, 0.0f, 80.0f }, 0.0f, 0.0f, 0.0f } };
		Expect(Cover(camera300, origin, Lens{ 60.0f, 2.0f, 0.0f, 0.0f }, high).worst <= 1.0f, "in frame without bars");
		Expect(Cover(camera300, origin, Lens{ 60.0f, 2.0f, 0.1f, 0.0f }, high).worst > 1.0f, "under the bar with them");
	}

	// ---- How far back --------------------------------------------------------

	{
		// Head and shoulders of someone standing still, from level: the chest decides.
		const std::array still{ FigureAt(origin, 42.0f) };
		const Mount      level{ origin, -1.5707963f, 0.0f };
		Expect(Near(DistanceToFit(level, wide60, still, limits, 100.0f), 42.0f / (0.57735f / 1.78f), 2.5f),
			"just far enough to get them in");
		Expect(Near(DistanceToFit(level, wide60, still, limits, 200.0f), 200.0f), "never closer than asked");

		const Mount low{ { 0.0f, 0.0f, 100.0f }, 0.0f, -0.5235988f, 50.0f };
		Expect(Near(Place(low, 200.0f).z, 50.0f), "a low camera stops at the floor");
	}

	constexpr Lens shot{ 60.0f, 1.78f, 0.0f, 0.06f };
	const Point    camera200{ 0.0f, -200.0f, 0.0f };
	const auto     across = [](float a_half, int a_steps) {
		std::array<Sample, 9> samples{};
		for (int i = 0; i < a_steps; ++i) {
			const float x = -a_half + 2.0f * a_half * static_cast<float>(i) / static_cast<float>(a_steps - 1);
			samples[static_cast<std::size_t>(i)] = FigureAt({ x, 0.0f, 0.0f }, 42.0f);
		}
		return samples;
	};
	const auto best = [&](std::span<const Sample> a_samples) {
		std::array<Framing, kFramings> framings{};
		const auto count = Frames(camera200, origin, origin, -1.0e9f, 1.0e9f, shot, a_samples, limits, framings);
		Expect(count > 0, "some frame fits");
		for (std::size_t i = 1; i < count; ++i) {
			Expect(framings[i - 1].score >= framings[i].score, "best first");
		}
		for (std::size_t i = 0; i < count; ++i) {
			Expect(Fits(Cover(framings[i].camera, framings[i].aim, shot, a_samples), limits), "every offer holds all of it");
			Expect(framings[i].distance >= 200.0f - 0.01f, "never closer than the shot as designed");
		}
		return framings[0];
	};
	{
		// Standing still: the shot as designed, from its own angle.
		const std::array still{ FigureAt(origin, 42.0f), FigureAt(origin, 42.0f) };
		const auto       pick = best(still);
		Expect(pick.turn == 0.0f && Near(pick.distance, 200.0f), "nothing to make room for");
	}
	{
		// A little movement keeps it close; a lot pushes it back.
		const auto small = across(30.0f, 3);
		const auto some = across(100.0f, 5);
		const auto lots = across(300.0f, 7);
		const auto smallPick = best(std::span{ small.data(), 3 });
		const auto somePick = best(std::span{ some.data(), 5 });
		const auto lotsPick = best(std::span{ lots.data(), 7 });
		Expect(Near(smallPick.distance, 200.0f), "a few steps fit the shot as it is");
		Expect(somePick.distance > 200.0f && somePick.distance < 300.0f, "a small area moves it back a little");
		Expect(lotsPick.distance > 500.0f, "a walk across the room moves it well back");
	}
	{
		// Walking toward the camera never brings them inside the nearest allowed.
		std::array<Sample, 4> toward{};
		for (int i = 0; i < 4; ++i) {
			toward[static_cast<std::size_t>(i)] = FigureAt({ 0.0f, -50.0f * static_cast<float>(i), 0.0f }, 42.0f);
		}
		const auto pick = best(toward);
		Expect(Cover(pick.camera, pick.aim, shot, toward).nearest >= limits.minApproach, "kept off the lens");
	}
	{
		const std::array spread{ Sample{ origin }, Sample{ { 100.0f, 0.0f, 10.0f } }, Sample{ { 100.0f, 50.0f, 20.0f } } };
		const auto       centre = Centre(spread);
		Expect(Near(centre.x, 50.0f) && Near(centre.y, 25.0f) && Near(centre.z, 10.0f), "the middle of the movement");
		Expect(Near(Spread(spread), 111.8f), "how far it spreads");
	}

	// ---- Does the shot as designed already hold it ---------------------------

	{
		constexpr Lens design{ 60.0f, 1.78f, 0.0f, 0.05f };
		const Point    camera150{ 0.0f, -150.0f, 0.0f };
		const std::array still{ FaceAt(origin, 42.0f) };
		Expect(FitsAsDesigned(camera150, origin, origin, design, still, limits), "standing still always fits");

		const std::array sideways{ FaceAt(origin, 42.0f), FaceAt({ 150.0f, 0.0f, 0.0f }, 42.0f) };
		Expect(!FitsAsDesigned(camera150, origin, origin, design, sideways, limits), "a walk out of the side doesn't");

		const std::array toward{ FaceAt(origin, 42.0f), FaceAt({ 0.0f, -120.0f, 0.0f }, 42.0f) };
		Expect(!FitsAsDesigned(camera150, origin, origin, design, toward, limits), "nor does one into the lens");
	}

	// ---- Walks that never start ----------------------------------------------

	{
		Doubt doubt{};
		const Point here{ 10.0f, 20.0f, 0.0f };
		Expect(Believe(doubt, 0xFF0046B7u, here, 5.0f, false), "a new forecast is believed");
		Expect(Believe(doubt, 0xFF0046B7u, here, 5.0f + kDoubtSeconds - 0.1f, false), "for a while");
		Expect(!Believe(doubt, 0xFF0046B7u, here, 5.0f + kDoubtSeconds, false), "not once it's overdue");
		Expect(!Believe(doubt, 0xFF0046B7u, { 30.0f, 20.0f, 0.0f }, 20.0f, false), "a shuffle doesn't redeem it");

		Expect(Believe(doubt, 0xFF0046B7u, here, 21.0f, true), "walking does");
		Expect(Believe(doubt, 0xFF0046B7u, here, 22.0f, false), "and starts the wait again");

		Expect(!Believe(doubt, 0xFF0046B7u, here, 30.0f, false), "overdue again");
		Expect(Believe(doubt, 0x0002C30Bu, here, 30.0f, false), "a different package is a new forecast");
		Expect(!Believe(doubt, 0x0002C30Bu, here, 30.0f + kDoubtSeconds, false), "with its own wait");
		Expect(Believe(doubt, 0x0002C30Bu, { 10.0f, 20.0f + kDoubtTravel, 0.0f }, 40.0f, false),
			"getting somewhere is a new start");
	}

	// ---- Losing them ---------------------------------------------------------

	{
		const std::array here{ FaceAt(origin, 42.0f) };
		const std::array gone{ FaceAt({ 250.0f, 0.0f, 0.0f }, 42.0f) };
		const std::array close{ FaceAt({ 0.0f, -250.0f, 0.0f }, 42.0f) };
		Expect(Check(camera300, origin, wide60, here, limits) == Lost::kNo, "still in frame");
		Expect(Check(camera300, origin, wide60, gone, limits) == Lost::kOutOfFrame, "out of the side of the frame");
		Expect(Check(camera300, origin, wide60, close, limits) == Lost::kTooClose, "into the lens");
	}

	std::cout << "Tripod: all tests passed\n";
	return EXIT_SUCCESS;
}
