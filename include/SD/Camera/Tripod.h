#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace SD::Camera::Tripod
{
	// Still shots of people who move while a scene is filmed.
	//
	// Every shot is solved relative to its subject, so someone who walks drags the
	// camera along with them. A film camera is placed before they move, far enough
	// back that everywhere they're about to be is already in the frame, and then
	// neither the camera nor its aim moves. A few steps on the spot keep it close;
	// a walk across the room pushes it back, and may turn it to an angle the walk
	// fits better.
	//
	// This forecasts the movement and fits the frame to it. Pure, so it can be
	// tested; the Director gathers the evidence from the game.

	struct Point
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };
	};

	// What said they would move, most informed first.
	enum class Source : std::uint8_t
	{
		kNone,
		kScenePackage,  // the scene sends them somewhere, now or before their next line
		kPackage,       // their own AI package has a destination away from them
		kVelocity,      // already walking; extrapolated in a straight line
	};

	[[nodiscard]] constexpr std::string_view SourceName(Source a_source) noexcept
	{
		switch (a_source) {
		case Source::kScenePackage: return "the scene's package";
		case Source::kPackage:      return "their AI package";
		case Source::kVelocity:     return "how they're moving";
		default:                    return "nothing";
		}
	}

	// A typical NPC walk, in units per second. Used to size the walk when there is
	// a destination but no measured speed yet.
	inline constexpr float kWalkSpeed = 90.0f;

	// Below this they're shifting their weight, not walking.
	inline constexpr float kMovingSpeed = 30.0f;

	// A move shorter than this doesn't need a different camera.
	inline constexpr float kMinTravel = 120.0f;

	// How far ahead a forecast looks. A destination is trusted for the length of a
	// shot; a velocity only for a few seconds, since people turn.
	inline constexpr float kHorizonSeconds = 6.0f;
	inline constexpr float kVelocityHorizonSeconds = 3.0f;

	// Someone headed for a person stops about a conversation's distance short of
	// them, not on top of them.
	inline constexpr float kMeetDistance = 150.0f;

	// A package says where someone belongs, not that they're on their way. A walk
	// that still hasn't started this long after it was first forecast, with them
	// standing where they were, isn't believed until they move or the package
	// changes.
	inline constexpr float kDoubtSeconds = 1.5f;
	inline constexpr float kDoubtTravel = 40.0f;

	struct Evidence
	{
		Point position{};  // their root now
		Point velocity{};  // measured, units per second

		bool   hasDestination{ false };
		Point  destination{};
		float  arriveRadius{ 0.0f };  // a package is satisfied anywhere inside this
		Source destinationSource{ Source::kNone };
	};

	struct Forecast
	{
		bool   moving{ false };
		Source source{ Source::kNone };
		Point  from{};
		Point  to{};                 // where to expect them by the end of the shot
		float  distance{ 0.0f };     // flat, from `from` to `to`
		float  remaining{ 0.0f };    // flat, to the destination's edge (0 without one)
		bool   reaches{ false };     // `to` is the destination itself
	};

	[[nodiscard]] inline float Flat(const Point& a_from, const Point& a_to) noexcept
	{
		const float dx = a_to.x - a_from.x;
		const float dy = a_to.y - a_from.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	[[nodiscard]] inline Forecast Predict(const Evidence& a_evidence) noexcept
	{
		Forecast forecast{};
		forecast.from = a_evidence.position;
		forecast.to = a_evidence.position;

		const float speed = std::sqrt(a_evidence.velocity.x * a_evidence.velocity.x +
		                              a_evidence.velocity.y * a_evidence.velocity.y);

		if (a_evidence.hasDestination) {
			const float toCentre = Flat(a_evidence.position, a_evidence.destination);
			const float remaining = toCentre - std::max(a_evidence.arriveRadius, 0.0f);
			forecast.remaining = std::max(remaining, 0.0f);
			if (remaining >= kMinTravel && toCentre > 1.0f) {
				// Already walking: their own pace. Otherwise an ordinary walk.
				const float pace = speed >= kMovingSpeed ? speed : kWalkSpeed;
				const float travel = std::min(remaining, pace * kHorizonSeconds);
				const float t = travel / toCentre;
				forecast.moving = true;
				forecast.source = a_evidence.destinationSource;
				forecast.to = {
					a_evidence.position.x + (a_evidence.destination.x - a_evidence.position.x) * t,
					a_evidence.position.y + (a_evidence.destination.y - a_evidence.position.y) * t,
					a_evidence.position.z + (a_evidence.destination.z - a_evidence.position.z) * t
				};
				forecast.distance = travel;
				forecast.reaches = travel >= remaining;
				return forecast;
			}
			// A destination they're already at says they'll stay, whatever their feet
			// are doing this frame.
			if (a_evidence.destinationSource == Source::kScenePackage) {
				return forecast;
			}
		}

		const float travel = speed * kVelocityHorizonSeconds;
		if (speed >= kMovingSpeed && travel >= kMinTravel) {
			forecast.moving = true;
			forecast.source = Source::kVelocity;
			forecast.to = {
				a_evidence.position.x + a_evidence.velocity.x * kVelocityHorizonSeconds,
				a_evidence.position.y + a_evidence.velocity.y * kVelocityHorizonSeconds,
				a_evidence.position.z
			};
			forecast.distance = travel;
		}
		return forecast;
	}

	// The walk one package has been forecasting for someone, and since when.
	struct Doubt
	{
		std::uint32_t key{ 0 };       // the package
		Point         from{};         // where they stood when it first did
		float         since{ 0.0f };  // on the caller's clock, in seconds
	};

	// Called whenever a_key forecasts a walk. False once that walk is overdue:
	// they've stood still against it for kDoubtSeconds. Walking, getting
	// somewhere, or a different package starts the wait again.
	[[nodiscard]] inline bool Believe(Doubt& a_doubt, std::uint32_t a_key, const Point& a_position, float a_now,
		bool a_walking) noexcept
	{
		if (a_walking || a_doubt.key != a_key || Flat(a_doubt.from, a_position) >= kDoubtTravel) {
			a_doubt = { a_key, a_position, a_now };
			return true;
		}
		return a_now - a_doubt.since < kDoubtSeconds;
	}

	// Degrees between two bearings out of a point, flat.
	[[nodiscard]] inline float Turn(const Point& a_from, const Point& a_first, const Point& a_second) noexcept
	{
		const float ax = a_first.x - a_from.x;
		const float ay = a_first.y - a_from.y;
		const float bx = a_second.x - a_from.x;
		const float by = a_second.y - a_from.y;
		const float la = std::sqrt(ax * ax + ay * ay);
		const float lb = std::sqrt(bx * bx + by * by);
		if (la < 1.0e-3f || lb < 1.0e-3f) {
			return 180.0f;
		}
		const float c = std::clamp((ax * bx + ay * by) / (la * lb), -1.0f, 1.0f);
		return std::acos(c) * 57.29578f;
	}

	[[nodiscard]] inline float Distance(const Point& a_from, const Point& a_to) noexcept
	{
		const float dx = a_to.x - a_from.x;
		const float dy = a_to.y - a_from.y;
		const float dz = a_to.z - a_from.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	// ---- Fitting a still frame ----------------------------------------------

	// Somewhere the subject is going to be, as the point their eyes will be at,
	// with how much of them has to stay in frame around it.
	struct Sample
	{
		Point head{};
		float above{ 0.0f };
		float below{ 0.0f };
		float side{ 0.0f };  // half their width
	};

	// The face: what has to stay in frame for the shot to still be of them. Sized
	// against Anatomy::extent, which is head and shoulders on a person.
	[[nodiscard]] inline Sample FaceAt(const Point& a_head, float a_extent) noexcept
	{
		return { a_head, a_extent * 0.3f, a_extent * 0.3f, a_extent * 0.3f };
	}

	// Head and shoulders down to the chest: the least a frame fitted to someone
	// walking shows of them.
	[[nodiscard]] inline Sample FigureAt(const Point& a_head, float a_extent) noexcept
	{
		return { a_head, a_extent * 0.35f, a_extent, a_extent * 0.55f };
	}

	struct Lens
	{
		float fovDegrees{ 75.0f };  // horizontal
		float aspect{ 1.78f };
		float crop{ 0.0f };    // share of the height under each black bar
		float margin{ 0.0f };  // share of each half kept clear inside the edges
	};

	struct Coverage
	{
		float worst{ 0.0f };     // how far toward an edge the furthest sample reaches; over 1 is outside
		float nearest{ 0.0f };   // depth of the nearest head
		float farthest{ 0.0f };  // and of the furthest
		bool  behind{ false };   // some of it is level with or behind the lens
	};

	// Where the samples land in the frame of a camera at a_camera looking at
	// a_aim. Right and up are built the way the camera node's are (ApplyPose).
	[[nodiscard]] inline Coverage Cover(const Point& a_camera, const Point& a_aim, const Lens& a_lens,
		std::span<const Sample> a_samples) noexcept
	{
		Coverage coverage{};
		float    fx = a_aim.x - a_camera.x;
		float    fy = a_aim.y - a_camera.y;
		float    fz = a_aim.z - a_camera.z;
		const float length = std::sqrt(fx * fx + fy * fy + fz * fz);
		if (length < 1.0e-3f || a_samples.empty()) {
			coverage.behind = true;
			coverage.worst = 1.0e9f;
			return coverage;
		}
		fx /= length;
		fy /= length;
		fz /= length;

		float       rx = fy;
		float       ry = -fx;
		const float rl = std::sqrt(rx * rx + ry * ry);
		if (rl < 1.0e-4f) {
			rx = 1.0f;
			ry = 0.0f;
		} else {
			rx /= rl;
			ry /= rl;
		}
		const float ux = ry * fz;
		const float uy = -rx * fz;
		const float uz = rx * fy - ry * fx;

		const float half = std::tan(std::clamp(a_lens.fovDegrees, 1.0f, 170.0f) * 0.5f * 0.01745329f);
		const float keep = 1.0f - std::clamp(a_lens.margin, 0.0f, 0.9f);
		const float xMax = half * keep;
		const float yMax = half / std::max(a_lens.aspect, 0.1f) * (1.0f - 2.0f * std::clamp(a_lens.crop, 0.0f, 0.45f)) * keep;

		coverage.nearest = 1.0e9f;
		for (const auto& sample : a_samples) {
			const auto depthOf = [&](float a_rise) {
				return (sample.head.x - a_camera.x) * fx + (sample.head.y - a_camera.y) * fy +
				       (sample.head.z + a_rise - a_camera.z) * fz;
			};
			const auto heightOf = [&](float a_rise) {
				return (sample.head.x - a_camera.x) * ux + (sample.head.y - a_camera.y) * uy +
				       (sample.head.z + a_rise - a_camera.z) * uz;
			};

			const float depth = depthOf(0.0f);
			const float top = depthOf(sample.above);
			const float bottom = depthOf(-sample.below);
			if (depth <= 1.0f || top <= 1.0f || bottom <= 1.0f) {
				coverage.behind = true;
				coverage.worst = 1.0e9f;
				continue;
			}
			coverage.nearest = std::min(coverage.nearest, depth);
			coverage.farthest = std::max(coverage.farthest, depth);

			const float across = (sample.head.x - a_camera.x) * rx + (sample.head.y - a_camera.y) * ry;
			const float wide = (std::abs(across) + sample.side) / depth / xMax;
			const float tall = std::max(std::abs(heightOf(sample.above)) / top,
								   std::abs(heightOf(-sample.below)) / bottom) / yMax;
			coverage.worst = std::max({ coverage.worst, wide, tall });
		}
		return coverage;
	}

	struct Limits
	{
		float minApproach{ 90.0f };    // nobody comes nearer the lens than this
		float maxSizeChange{ 2.6f };   // the furthest head at most this much further away than the nearest
		float maxDistance{ 1600.0f };  // camera to aim
	};

	[[nodiscard]] inline bool Fits(const Coverage& a_coverage, const Limits& a_limits) noexcept
	{
		return !a_coverage.behind && a_coverage.worst <= 1.0f && a_coverage.nearest >= a_limits.minApproach &&
		       a_coverage.farthest <= a_coverage.nearest * a_limits.maxSizeChange;
	}

	// Whether the shot as designed already holds the movement: their face stays in
	// its frame everywhere they're going to be, and nobody comes much nearer the
	// lens than they are now. A tight shot that crops them on purpose isn't judged
	// on that, only on the face.
	[[nodiscard]] inline bool FitsAsDesigned(const Point& a_camera, const Point& a_aim, const Point& a_from,
		const Lens& a_lens, std::span<const Sample> a_faces, const Limits& a_limits) noexcept
	{
		const auto coverage = Cover(a_camera, a_aim, a_lens, a_faces);
		const float now = Distance(a_camera, a_from);
		return !coverage.behind && coverage.worst <= 1.0f &&
		       coverage.nearest >= 0.8f * std::min(now, a_limits.minApproach);
	}

	// Where a camera stands for a given distance: out from the aim along a
	// bearing and elevation, kept between a floor and a ceiling.
	struct Mount
	{
		Point aim{};
		float bearing{ 0.0f };    // radians, from the aim toward the camera
		float elevation{ 0.0f };  // radians, positive above the aim
		float floor{ -1.0e9f };
		float ceiling{ 1.0e9f };
	};

	[[nodiscard]] inline Point Place(const Mount& a_mount, float a_distance) noexcept
	{
		const float flat = std::cos(a_mount.elevation) * a_distance;
		Point       camera{ a_mount.aim.x + std::cos(a_mount.bearing) * flat,
				  a_mount.aim.y + std::sin(a_mount.bearing) * flat,
				  a_mount.aim.z + std::sin(a_mount.elevation) * a_distance };
		camera.z = std::clamp(camera.z, a_mount.floor, std::max(a_mount.floor, a_mount.ceiling));
		return camera;
	}

	// The nearest distance from the aim, no closer than a_minDistance, at which
	// every sample fits. -1 when even the furthest allowed doesn't.
	[[nodiscard]] inline float DistanceToFit(const Mount& a_mount, const Lens& a_lens, std::span<const Sample> a_samples,
		const Limits& a_limits, float a_minDistance) noexcept
	{
		const auto fits = [&](float a_distance) {
			return Fits(Cover(Place(a_mount, a_distance), a_mount.aim, a_lens, a_samples), a_limits);
		};
		const float most = a_limits.maxDistance;
		float       low = std::clamp(a_minDistance, 1.0f, most);
		if (fits(low)) {
			return low;
		}
		if (!fits(most)) {
			return -1.0f;
		}
		float high = most;
		for (int i = 0; i < 24 && high - low > 2.0f; ++i) {
			const float middle = (low + high) * 0.5f;
			(fits(middle) ? high : low) = middle;
		}
		return high;
	}

	// The middle of everywhere the heads will be: the centre of their flat
	// extent, at their mean height.
	[[nodiscard]] inline Point Centre(std::span<const Sample> a_samples) noexcept
	{
		if (a_samples.empty()) {
			return {};
		}
		float minX = a_samples[0].head.x;
		float maxX = minX;
		float minY = a_samples[0].head.y;
		float maxY = minY;
		float z = 0.0f;
		for (const auto& sample : a_samples) {
			minX = std::min(minX, sample.head.x);
			maxX = std::max(maxX, sample.head.x);
			minY = std::min(minY, sample.head.y);
			maxY = std::max(maxY, sample.head.y);
			z += sample.head.z;
		}
		return { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, z / static_cast<float>(a_samples.size()) };
	}

	// How far across the samples spread, flat. For the log.
	[[nodiscard]] inline float Spread(std::span<const Sample> a_samples) noexcept
	{
		float most = 0.0f;
		for (const auto& a : a_samples) {
			for (const auto& b : a_samples) {
				most = std::max(most, Flat(a.head, b.head));
			}
		}
		return most;
	}

	// The shot's own angle first, then turned either way. A turned angle has to
	// frame them about a tenth larger to win, the furthest about a third: a
	// different perspective is taken when the movement asks for it.
	inline constexpr std::array kTurnDegrees{ 0.0f, 35.0f, -35.0f, 70.0f, -70.0f };
	inline constexpr std::array kTurnPreference{ 1.0f, 0.9f, 0.9f, 0.75f, 0.75f };
	inline constexpr std::size_t kFramings = kTurnDegrees.size();

	struct Framing
	{
		Point camera{};
		Point aim{};
		float distance{ 0.0f };  // camera to aim
		float turn{ 0.0f };      // degrees off the shot's own bearing
		float size{ 0.0f };      // how big they are now, against the shot as designed
		float score{ 0.0f };
	};

	// Still frames that hold every sample, best first. Each keeps the designed
	// shot's height and the offset of its aim from the subject, moves the aim to
	// the middle of the movement, and comes no closer than the shot itself.
	// a_from is where the subject is now; "how big" is measured there.
	[[nodiscard]] inline std::size_t Frames(const Point& a_designCamera, const Point& a_designAim, const Point& a_from,
		float a_floor, float a_ceiling, const Lens& a_lens, std::span<const Sample> a_samples, const Limits& a_limits,
		std::span<Framing> a_out) noexcept
	{
		const float dx = a_designCamera.x - a_designAim.x;
		const float dy = a_designCamera.y - a_designAim.y;
		const float dz = a_designCamera.z - a_designAim.z;
		const float designDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (designDistance < 1.0f || a_samples.empty()) {
			return 0;
		}
		const float bearing = std::atan2(dy, dx);
		const float elevation = std::asin(std::clamp(dz / designDistance, -1.0f, 1.0f));
		const float designDepth = std::max(Distance(a_designCamera, a_from), 1.0f);

		const auto  centre = Centre(a_samples);
		const Point aim{ a_designAim.x + centre.x - a_from.x, a_designAim.y + centre.y - a_from.y,
			a_designAim.z + centre.z - a_from.z };

		std::size_t count = 0;
		for (std::size_t i = 0; i < kTurnDegrees.size() && count < a_out.size(); ++i) {
			const Mount mount{ aim, bearing + kTurnDegrees[i] * 0.01745329f, elevation, a_floor, a_ceiling };
			const float distance = DistanceToFit(mount, a_lens, a_samples, a_limits, designDistance);
			if (distance < 0.0f) {
				continue;
			}
			Framing framing{};
			framing.camera = Place(mount, distance);
			framing.aim = aim;
			framing.distance = distance;
			framing.turn = kTurnDegrees[i];
			framing.size = designDepth / std::max(Distance(framing.camera, a_from), 1.0f);
			framing.score = framing.size * kTurnPreference[i];
			a_out[count++] = framing;
		}
		std::sort(a_out.begin(), a_out.begin() + static_cast<std::ptrdiff_t>(count),
			[](const Framing& a_lhs, const Framing& a_rhs) { return a_lhs.score > a_rhs.score; });
		return count;
	}

	// Why a still frame has to give up on its subject. Line of sight is the
	// Director's, since it needs the physics world.
	enum class Lost : std::uint8_t
	{
		kNo,
		kOutOfFrame,  // their face reached the edge of the frame
		kTooClose,    // they walked into the lens
	};

	[[nodiscard]] constexpr std::string_view LostName(Lost a_lost) noexcept
	{
		switch (a_lost) {
		case Lost::kOutOfFrame: return "moved out of frame";
		case Lost::kTooClose:   return "walked into the lens";
		default:                return "still in frame";
		}
	}

	// a_faces is where they are now, without a margin in a_lens: the cut comes when
	// the face actually reaches the edge.
	[[nodiscard]] inline Lost Check(const Point& a_camera, const Point& a_aim, const Lens& a_lens,
		std::span<const Sample> a_faces, const Limits& a_limits) noexcept
	{
		const auto coverage = Cover(a_camera, a_aim, a_lens, a_faces);
		if (coverage.behind || coverage.nearest < a_limits.minApproach * 0.75f) {
			return Lost::kTooClose;
		}
		if (coverage.worst > 1.0f) {
			return Lost::kOutOfFrame;
		}
		return Lost::kNo;
	}
}
