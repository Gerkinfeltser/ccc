#include "SD/Scene/OverlaySignal.h"

#include "SD/Core/Logging.h"

namespace SD::Scene
{
	namespace
	{
		constexpr std::string_view kGlobal = "PhotoMode_IsActive"sv;
		constexpr float            kUp = 1.0f;

		RE::TESGlobal* global = nullptr;
		bool           raised = false;
		float          before = 0.0f;

		// A global nobody loaded, reachable by editor ID like one from a plugin. It isn't
		// in the form map, so it has no form ID and the save never sees it.
		[[nodiscard]] RE::TESGlobal* Make()
		{
			auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESGlobal>();
			auto* made = factory ? factory->Create() : nullptr;
			if (!made) {
				return nullptr;
			}
			made->type = RE::TESGlobal::Type::kShort;
			made->value = 0.0f;
			made->SetFormEditorID(kGlobal.data());

			const auto& [map, lock] = RE::TESForm::GetAllFormsByEditorID();
			if (!map) {
				return nullptr;
			}
			const RE::BSWriteLockGuard guard{ lock.get() };
			map->insert(RE::BSTTuple<const RE::BSFixedString, RE::TESForm*>{ RE::BSFixedString{ kGlobal },
				static_cast<RE::TESForm*>(made) });
			return made;
		}
	}

	void OverlaySignal::Prepare()
	{
		if (global) {
			return;
		}
		global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(kGlobal);
		if (global) {
			const auto* file = global->GetFile(0);
			Log::Info(Log::Category::kStaging,
				"Overlay signal: {} from {}. Mods that hide for Photo Mode hide while other people's "
				"conversations are filmed."sv,
				kGlobal, file ? file->GetFilename() : "the game"sv);
			return;
		}
		global = Make();
		if (global) {
			Log::Info(Log::Category::kStaging,
				"Overlay signal: made {} (Photo Mode isn't installed). Mods that hide for Photo Mode hide while "
				"other people's conversations are filmed."sv,
				kGlobal);
		} else {
			Log::Warn(Log::Category::kStaging,
				"Overlay signal: couldn't make {}. Overlays drawn by other mods (such as Custom Markers' mini "
				"map) stay up while other people's conversations are filmed."sv,
				kGlobal);
		}
	}

	void OverlaySignal::Raise()
	{
		if (!global) {
			return;
		}
		if (!raised) {
			before = global->value;
			raised = true;
			Log::Info(Log::Category::kStaging, "Overlay signal up: {} = 1 (was {})."sv, kGlobal, before);
		}
		if (global->value != kUp) {
			global->value = kUp;
		}
	}

	void OverlaySignal::Lower()
	{
		if (!global || !raised) {
			return;
		}
		// Photo Mode may have set it itself meanwhile; it's put back only if it's still
		// ours.
		if (global->value == kUp) {
			global->value = before;
		}
		raised = false;
		Log::Info(Log::Category::kStaging, "Overlay signal down: {} = {}."sv, kGlobal, global->value);
	}

	void OverlaySignal::BeforeSave()
	{
		if (global && raised && global->value == kUp) {
			global->value = before;
		}
	}
}
