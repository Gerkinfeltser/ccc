#include "SD/Scene/CloseMic.h"

#include "SD/Core/Logging.h"

namespace SD::Scene
{
	namespace
	{
		// In front of the camera, well inside the 70-unit minimum distance of the
		// default dialogue output model, and far enough out that the voice comes from
		// the screen rather than from inside the listener.
		constexpr float kAhead = 40.0f;

		struct Line
		{
			RE::ActorHandle speaker;
			std::uint32_t   soundID{ RE::BSSoundHandle::kInvalidID };
		};

		bool              enabled{ true };
		std::vector<Line> lines;
		std::uint32_t     moved{ 0 };

		// Sounds already known to be over. An actor keeps a finished line's ID in
		// its handle until the next line replaces it, and asking the audio manager
		// about one takes its lock, so without this every silent actor in the
		// scene costs two locked lookups a frame.
		std::array<std::uint32_t, 32> spent = [] {
			std::array<std::uint32_t, 32> none{};
			none.fill(RE::BSSoundHandle::kInvalidID);
			return none;
		}();
		std::size_t spentNext{ 0 };

		// A sound the audio manager doesn't know yet may simply not have started:
		// the handle is filled in before the audio thread registers it. So an
		// unknown sound is asked about again for this many frames before it is
		// written off.
		constexpr std::uint8_t kDoubtFrames = 30;
		struct Doubt
		{
			std::uint32_t soundID{ RE::BSSoundHandle::kInvalidID };
			std::uint8_t  frames{ 0 };
		};
		std::array<Doubt, 16> doubts{};
		std::size_t           doubtNext{ 0 };

		// Finished lines are only looked for this often; nothing is waiting on it.
		constexpr std::uint32_t kPruneEvery = 8;
		std::uint32_t           updates{ 0 };

		[[nodiscard]] bool Spent(std::uint32_t a_soundID)
		{
			return std::ranges::find(spent, a_soundID) != spent.end();
		}

		void Retire(std::uint32_t a_soundID)
		{
			spent[spentNext] = a_soundID;
			spentNext = (spentNext + 1) % spent.size();
		}

		// Counts a frame on which the audio manager didn't know this sound, and
		// writes it off once that has gone on long enough.
		void DoubtIt(std::uint32_t a_soundID)
		{
			for (auto& doubt : doubts) {
				if (doubt.soundID == a_soundID) {
					if (++doubt.frames >= kDoubtFrames) {
						Retire(a_soundID);
						doubt = {};
					}
					return;
				}
			}
			doubts[doubtNext] = { a_soundID, 1 };
			doubtNext = (doubtNext + 1) % doubts.size();
		}

		// Created once and never freed: a static smart pointer would release it at
		// process exit, after the game's heap has gone.
		RE::NiNode* mic{ nullptr };

		[[nodiscard]] RE::NiNode* Mic()
		{
			if (!mic) {
				mic = RE::NiNode::Create(0);
				if (mic) {
					mic->IncRefCount();
				}
			}
			return mic;
		}

		// Where the engine attaches a line when it starts one: the head for an NPC, the
		// current 3D for the player (1.5.97, ID 36541).
		[[nodiscard]] RE::NiAVObject* HomeNode(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return nullptr;
			}
			if (!a_actor->IsPlayerRef()) {
				auto* process = a_actor->GetActorRuntimeData().currentProcess;
				if (auto* middle = process ? process->middleHigh : nullptr; middle && middle->headNode) {
					return middle->headNode;
				}
			}
			return a_actor->Get3D2();
		}

		// Points a sound at a different node. The old registration is removed first
		// rather than trusting a second one to replace it. Both are messages on the
		// audio manager's queue, so the audio thread sees them in this order.
		void Follow(std::uint32_t a_soundID, RE::NiAVObject* a_node)
		{
			auto* audio = RE::BSAudioManager::GetSingleton();
			if (!audio || !a_node || a_soundID == RE::BSSoundHandle::kInvalidID) {
				return;
			}
			audio->ComposeMessage(RE::SOUND_MSG::UnRegisterMovingSound, a_soundID);

			RE::BSSoundHandle handle;
			handle.soundID = a_soundID;
			handle.SetObjectToFollow(a_node);
		}

		[[nodiscard]] RE::BSSoundHandle* VoiceHandles(RE::Actor* a_actor)
		{
			auto* process = a_actor ? a_actor->GetActorRuntimeData().currentProcess : nullptr;
			auto* high = process ? process->high : nullptr;
			return high ? high->soundHandles : nullptr;
		}

		[[nodiscard]] bool Tracked(std::uint32_t a_soundID)
		{
			return std::ranges::any_of(lines, [a_soundID](const Line& a_line) { return a_line.soundID == a_soundID; });
		}
	}

	void CloseMic::SetEnabled(bool a_enabled) noexcept
	{
		enabled = a_enabled;
	}

	bool CloseMic::Enabled() noexcept
	{
		return enabled;
	}

	void CloseMic::Place(const RE::NiPoint3& a_position, const RE::NiPoint3& a_forward)
	{
		if (!enabled) {
			return;
		}
		auto* node = Mic();
		if (!node) {
			return;
		}
		// Read by the audio thread from world.translate. Nothing parents this node, so
		// local is kept the same.
		const RE::NiPoint3 at{ a_position.x + a_forward.x * kAhead, a_position.y + a_forward.y * kAhead,
			a_position.z + a_forward.z * kAhead };
		node->world.translate = at;
		node->local.translate = at;
	}

	void CloseMic::Update(std::span<RE::Actor* const> a_actors)
	{
		if (!enabled) {
			if (!lines.empty()) {
				Release();
			}
			return;
		}

		// The camera the listener is oriented from. Its X axis is the view direction.
		// ApplyPose places the microphone again later in the frame with the pose it
		// writes; this covers the frames where it doesn't run.
		if (auto* camera = RE::Main::WorldRootCamera()) {
			const auto& world = camera->world;
			Place(world.translate, { world.rotate.entry[0][0], world.rotate.entry[1][0], world.rotate.entry[2][0] });
		}
		auto* node = Mic();
		if (!node) {
			return;
		}

		// A line is forgotten once its sound has been released.
		if (++updates % kPruneEvery == 0 && !lines.empty()) {
			std::erase_if(lines, [](const Line& a_line) {
				RE::BSSoundHandle handle;
				handle.soundID = a_line.soundID;
				if (handle.IsValid()) {
					return false;
				}
				Retire(a_line.soundID);
				return true;
			});
		}

		for (auto* actor : a_actors) {
			auto* handles = VoiceHandles(actor);
			if (!handles) {
				continue;
			}
			for (std::size_t i = 0; i < 2; ++i) {
				auto& handle = handles[i];
				const auto id = handle.soundID;
				// The cheap answers first; only a sound never seen before reaches the
				// audio manager.
				if (id == RE::BSSoundHandle::kInvalidID || Tracked(id) || Spent(id)) {
					continue;
				}
				if (!handle.IsValid()) {
					DoubtIt(id);
					continue;
				}
				Follow(id, node);
				lines.push_back({ actor->GetHandle(), id });

				if (moved++ == 0) {
					Log::Info(Log::Category::kStaging, "Voices up close: {}'s line moved in front of the camera."sv,
						actor->GetName() ? actor->GetName() : "<unnamed>");
				}
			}
		}
	}

	void CloseMic::Release()
	{
		std::uint32_t returned = 0;
		for (const auto& line : lines) {
			RE::BSSoundHandle handle;
			handle.soundID = line.soundID;
			if (!handle.IsValid()) {
				continue;
			}
			if (auto speaker = line.speaker.get()) {
				Follow(line.soundID, HomeNode(speaker.get()));
				++returned;
			}
		}
		if (moved > 0) {
			Log::Info(Log::Category::kStaging, "Voices up close: {} line(s) this conversation, {} handed back."sv, moved,
				returned);
		}
		lines.clear();
		moved = 0;
	}
}
