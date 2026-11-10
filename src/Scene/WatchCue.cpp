#include "SD/Scene/WatchCue.h"

#include "SD/Core/Logging.h"

// Third-party header that doesn't build at /W4 /WX, so its warnings are
// suppressed here.
#pragma warning(push, 0)
#include "SKSEMenuFramework.h"
#pragma warning(pop)

#include <chrono>
#include <numbers>

namespace SD::Scene
{
	namespace
	{
		namespace Im = ImGuiMCP;
		using Clock = std::chrono::steady_clock;

		// Written on the game's thread, read where the overlay draws.
		std::atomic<float>        screenX{ 0.5f };
		std::atomic<float>        screenY{ 0.5f };
		std::atomic<float>        progress{ 0.0f };
		std::atomic_bool          atCrosshair{ false };
		std::atomic<std::int64_t> shownAt{ 0 };

		constexpr float kAboveHead = 30.0f;     // world units over the head bone
		constexpr float kFreshSeconds = 0.2f;   // no Show for this long and it fades
		constexpr float kFadeSeconds = 0.18f;

		[[nodiscard]] std::int64_t Now()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
		}

		[[nodiscard]] Im::ImU32 White(float a_alpha)
		{
			const auto a = static_cast<Im::ImU32>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f);
			return (a << 24) | 0x00FFFFFFu;
		}

		[[nodiscard]] Im::ImU32 Black(float a_alpha)
		{
			return static_cast<Im::ImU32>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f) << 24;
		}

		// A camera, drawn from shapes so no icon font is needed: body, lens and
		// the viewfinder on top.
		void DrawCamera(Im::ImDrawList* a_draw, Im::ImVec2 a_at, float a_size, float a_alpha)
		{
			const float w = a_size * 0.56f;
			const float h = a_size * 0.36f;
			Im::ImDrawListManager::AddRectFilled(a_draw, { a_at.x - w * 0.42f, a_at.y - h - a_size * 0.16f },
				{ a_at.x - w * 0.05f, a_at.y - h + 1.0f }, White(a_alpha), a_size * 0.05f, 0);
			Im::ImDrawListManager::AddRectFilled(a_draw, { a_at.x - w, a_at.y - h }, { a_at.x + w, a_at.y + h },
				White(a_alpha), a_size * 0.12f, 0);
			Im::ImDrawListManager::AddCircleFilled(a_draw, a_at, a_size * 0.24f, Black(a_alpha * 0.85f), 20);
			Im::ImDrawListManager::AddCircleFilled(a_draw, a_at, a_size * 0.11f, White(a_alpha), 14);
		}

		void __stdcall Draw()
		{
			static float alpha = 0.0f;
			static float shownProgress = 0.0f;
			static auto  last = Clock::now();

			const auto  now = Clock::now();
			const float delta = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0f, 0.1f);
			last = now;

			const bool fresh = static_cast<float>(Now() - shownAt.load(std::memory_order_relaxed)) < kFreshSeconds * 1000.0f;
			const float wanted = progress.load(std::memory_order_relaxed);

			// Quieter while it is only saying "there is something to watch".
			const float target = !fresh ? 0.0f : (wanted > 0.0f ? 1.0f : 0.55f);
			alpha += std::clamp(target - alpha, -delta / kFadeSeconds, delta / kFadeSeconds);
			if (alpha <= 0.01f) {
				shownProgress = 0.0f;
				return;
			}
			if (fresh) {
				// Eased a little, so the ring sweeps rather than steps at low frame rates.
				shownProgress += (wanted - shownProgress) * std::min(delta * 18.0f, 1.0f);
			}

			auto* io = Im::GetIO();
			auto* draw = Im::GetBackgroundDrawList();
			if (!io || !draw) {
				return;
			}

			const float      u = io->DisplaySize.y / 1080.0f;
			const bool       centred = atCrosshair.load(std::memory_order_relaxed);
			const float      radius = (centred ? 21.0f : 17.0f) * u;
			const Im::ImVec2 at{ screenX.load(std::memory_order_relaxed) * io->DisplaySize.x,
				screenY.load(std::memory_order_relaxed) * io->DisplaySize.y };

			// A soft dark disc so it reads over snow and sky.
			Im::ImDrawListManager::AddCircleFilled(draw, at, radius + 5.0f * u, Black(alpha * 0.32f), 40);
			Im::ImDrawListManager::AddCircle(draw, at, radius, White(alpha * 0.28f), 48, 2.0f * u);

			if (shownProgress > 0.005f) {
				constexpr float kTop = -std::numbers::pi_v<float> / 2.0f;
				const float     sweep = std::clamp(shownProgress, 0.0f, 1.0f) * 2.0f * std::numbers::pi_v<float>;
				Im::ImDrawListManager::PathClear(draw);
				Im::ImDrawListManager::PathArcTo(draw, at, radius, kTop, kTop + sweep, 48);
				Im::ImDrawListManager::PathStroke(draw, White(alpha), 0, 3.0f * u);
			}

			DrawCamera(draw, at, radius, alpha * 0.95f);
		}
	}

	void WatchCue::Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			Log::Info(Log::Category::kCore,
				"SKSE Menu Framework isn't installed, so the watch sign has nothing to draw with."sv);
			return;
		}
		static auto* element = SKSEMenuFramework::AddHudElement(Draw);
		static_cast<void>(element);
	}

	void WatchCue::Show(float a_progress)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* view = RE::PlayerCamera::GetSingleton();
		if (!player || !view) {
			return;
		}

		if (view->IsInFirstPerson()) {
			screenX.store(0.5f, std::memory_order_relaxed);
			screenY.store(0.5f, std::memory_order_relaxed);
			atCrosshair.store(true, std::memory_order_relaxed);
		} else {
			// Over the head: the head bone where there is one, the body's height otherwise.
			static const RE::BSFixedString kHead{ "NPC Head [Head]" };
			auto                           point = player->GetPosition();
			point.z += 128.0f * player->GetScale();
			if (auto* model = player->Get3D()) {
				if (auto* head = model->GetObjectByName(kHead)) {
					point = head->world.translate;
				}
			}
			point.z += kAboveHead;

			auto* camera = RE::Main::WorldRootCamera();
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (!camera ||
				!RE::NiCamera::WorldPtToScreenPt3(camera->GetRuntimeData().worldToCam, camera->GetRuntimeData2().port,
					point, x, y, z, 1e-5f) ||
				!std::isfinite(x) || !std::isfinite(y) || z <= 0.0f || x < 0.03f || x > 0.97f || y < 0.04f || y > 0.97f) {
				return;  // off screen this frame; the sign fades
			}
			screenX.store(x, std::memory_order_relaxed);
			screenY.store(1.0f - y, std::memory_order_relaxed);
			atCrosshair.store(false, std::memory_order_relaxed);
		}

		progress.store(std::clamp(a_progress, 0.0f, 1.0f), std::memory_order_relaxed);
		shownAt.store(Now(), std::memory_order_relaxed);
	}
}
