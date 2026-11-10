#include "SD/Camera/WalkSource.h"

#include "SD/Dialogue/SceneScript.h"

#include <cstring>

namespace SD::Camera::WalkSource
{
	namespace
	{
		using LocationType = RE::PackageLocation::Type;

		struct Place
		{
			bool             valid{ false };
			RE::NiPoint3     point{};
			float            radius{ 0.0f };
			RE::FormID       target{ 0 };
			std::string_view how{};

			// "Stay where you are": a place, but never a destination.
			bool here{ false };
		};

		[[nodiscard]] Place AtRef(RE::TESObjectREFR* a_ref, float a_radius, std::string_view a_how)
		{
			if (!a_ref) {
				return {};
			}
			// Going to a person ends in front of them.
			const float radius = a_ref->As<RE::Actor>() ? std::max(a_radius, Tripod::kMeetDistance) : a_radius;
			return Place{ true, a_ref->GetPosition(), radius, a_ref->GetFormID(), a_how, false };
		}

		[[nodiscard]] std::string_view KindOf(const RE::TESPackage* a_package)
		{
			using Type = RE::PACKAGE_TYPE;
			switch (a_package->packData.packType.get()) {
			case Type::kFollow:          return "follow"sv;
			case Type::kEscort:          return "escort"sv;
			case Type::kEat:             return "eat"sv;
			case Type::kSleep:           return "sleep"sv;
			case Type::kWander:          return "wander"sv;
			case Type::kTravel:          return "travel"sv;
			case Type::kAccompany:       return "accompany"sv;
			case Type::kUseItemAt:       return "use item at"sv;
			case Type::kSandbox:         return "sandbox"sv;
			case Type::kPatrol:          return "patrol"sv;
			case Type::kGuard:           return "guard"sv;
			case Type::kDialogue:        return "dialogue"sv;
			case Type::kFind:            return "find"sv;
			case Type::kPackage:         return "templated"sv;
			case Type::kActivate:        return "activate"sv;
			case Type::kSpectator:       return "spectator"sv;
			case Type::kDoNothing:       return "do nothing"sv;
			case Type::kInGameDialogue:  return "in-game dialogue"sv;
			default:                     return "other"sv;
			}
		}

		// Packages that keep someone where they are, or only turn them toward
		// somebody, whatever their location says. The engine gives an NPC talking to
		// another a dialogue package whose location is the person they're talking
		// to; read as a destination, every conversation looked like a walk across
		// the gap between them.
		[[nodiscard]] bool GoesNowhere(RE::TESPackage* a_package)
		{
			if (skyrim_cast<RE::DialoguePackage*>(a_package)) {
				return true;
			}
			using Type = RE::PACKAGE_TYPE;
			switch (a_package->packData.packType.get()) {
			case Type::kDialogue:
			case Type::kInGameDialogue:
			case Type::kSpectator:
			case Type::kDoNothing:
				return true;
			default:
				break;
			}
			using Procedure = RE::PACKAGE_PROCEDURE_TYPE;
			switch (a_package->procedureType.get()) {
			case Procedure::kDialogue:
			case Procedure::kGreet:
			case Procedure::kObserveCombat:
			case Procedure::kObserveDialogue:
			case Procedure::kTalkToDead:
			case Procedure::kDoNothing:
			case Procedure::kWaitForDialogue:
			case Procedure::kKeepAnEyeOn:
				return true;
			default:
				return false;
			}
		}

		// A package location as a point. Only the kinds that name somewhere: a
		// reference, a linked reference, a quest alias, the actor's editor position,
		// and "near self". A cell, an object type or a location alias has no single
		// point to walk to.
		[[nodiscard]] Place Resolve(const RE::PackageLocation* a_location, RE::Actor* a_actor, RE::TESQuest* a_quest)
		{
			if (!a_location) {
				return {};
			}
			const float radius = static_cast<float>(a_location->rad);
			switch (a_location->locType.get()) {
			case LocationType::kNearReference:
				return AtRef(a_location->data.refHandle.get().get(), radius, "a reference"sv);
			case LocationType::kNearLinkedReference:
				{
					auto* keyword = a_location->data.object ? a_location->data.object->As<RE::BGSKeyword>() : nullptr;
					return AtRef(a_actor->GetLinkedRef(keyword), radius, "their linked reference"sv);
				}
			case LocationType::kAlias_Reference:
				{
					if (!a_quest) {
						return {};
					}
					// The alias ID sits where the union keeps a pointer.
					std::uint32_t alias = 0;
					std::memcpy(&alias, &a_location->data, sizeof(alias));
					return AtRef(a_quest->GetAliasedRef(alias).get().get(), radius, "a quest alias"sv);
				}
			case LocationType::kNearEditorLocation:
				{
					RE::NiPoint3 position{};
					RE::NiPoint3 rotation{};
					RE::TESForm* where = nullptr;
					if (!a_actor->GetEditorLocation(position, rotation, where, a_actor->GetParentCell())) {
						return {};
					}
					return Place{ true, position, radius, 0, "where they were placed"sv, false };
				}
			case LocationType::kNearSelf:
				return Place{ true, a_actor->GetPosition(), radius, 0, "where they stand"sv, true };
			default:
				return {};
			}
		}

		// The place a package sends someone. A package built from a template keeps its
		// places as Location inputs in its data list (Travel's "Place to Travel");
		// an old-style package has one in packLoc. A real place beats "stay here".
		[[nodiscard]] Place PlaceOf(RE::TESPackage* a_package, RE::Actor* a_actor, RE::TESQuest* a_fallbackQuest)
		{
			if (!a_package) {
				return {};
			}
			auto* quest = a_package->ownerQuest ? a_package->ownerQuest : a_fallbackQuest;

			Place      stay{};
			const auto consider = [&](const RE::PackageLocation* a_location) {
				const auto place = Resolve(a_location, a_actor, quest);
				if (!place.valid) {
					return false;
				}
				if (place.here) {
					if (!stay.valid) {
						stay = place;
					}
					return false;
				}
				stay = place;
				return true;
			};

			if (auto* custom = a_package->data ? skyrim_cast<RE::TESCustomPackageData*>(a_package->data) : nullptr) {
				const auto& list = custom->data;
				for (std::uint16_t i = 0; list.data && i < list.dataSize; ++i) {
					auto* entry = list.data[i];
					auto* location = entry ? skyrim_cast<RE::BGSPackageDataLocation*>(entry) : nullptr;
					if (location && consider(location->pointer)) {
						return stay;
					}
				}
			}
			if (a_package->packLoc) {
				static_cast<void>(consider(a_package->packLoc));
			}
			return stay;
		}

		// The scene's next move for this actor: a package action of theirs running
		// now, or starting before their next line.
		[[nodiscard]] Destination FromScene(RE::BGSScene* a_scene, RE::Actor* a_actor)
		{
			const std::uint32_t phase = a_scene->currentPhaseIndex;
			const auto          count = static_cast<std::uint32_t>(a_scene->phases.size());
			auto*               quest = a_scene->parentQuest;
			if (!quest || phase == 0xFFFFFFFFu || phase >= count) {
				return {};
			}

			std::vector<Dialogue::ScriptLine>              lines;
			std::vector<Dialogue::ScriptMove>              moves;
			std::vector<const RE::BGSSceneActionPackage*> actions;
			for (const auto* action : a_scene->actions) {
				if (!action) {
					continue;
				}
				const auto type = action->GetType();
				if (type == RE::BGSSceneAction::Type::kDialogue) {
					if (static_cast<const RE::BGSSceneActionDialogue*>(action)->topic) {
						lines.push_back({ action->startPhase, action->endPhase,
							action->flags.all(RE::BGSSceneAction::Flag::kLooping) });
					}
				} else if (type == RE::BGSSceneAction::Type::kPackage) {
					// The action names an alias of the scene's quest; only this actor's count.
					if (quest->GetAliasedRef(action->actorID).get().get() != a_actor) {
						continue;
					}
					moves.push_back({ action->startPhase, action->endPhase });
					actions.push_back(static_cast<const RE::BGSSceneActionPackage*>(action));
				}
			}

			const auto until = Dialogue::LookAheadEnd(lines, phase, count);
			while (!moves.empty()) {
				const int index = Dialogue::FirstMove(moves, phase, until);
				if (index < 0) {
					break;
				}
				const auto* action = actions[static_cast<std::size_t>(index)];
				for (auto* package : action->packages) {
					if (!package || GoesNowhere(package)) {
						continue;
					}
					const auto place = PlaceOf(package, a_actor, quest);
					if (!place.valid) {
						continue;
					}
					Destination found{};
					found.valid = true;
					found.point = place.point;
					found.radius = place.radius;
					found.source = Tripod::Source::kScenePackage;
					found.package = package->GetFormID();
					found.target = place.target;
					found.scene = a_scene->GetFormID();
					found.phase = std::max<std::uint32_t>(action->startPhase, phase);
					found.later = action->startPhase > phase;
					found.how = place.how;
					found.kind = KindOf(package);
					return found;
				}
				// Nothing in it names a place; try the next.
				moves.erase(moves.begin() + index);
				actions.erase(actions.begin() + index);
			}
			return {};
		}
	}

	Destination Find(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}

		if (auto* scene = a_actor->GetCurrentScene(); scene && scene->isPlaying) {
			if (auto found = FromScene(scene, a_actor); found.valid) {
				return found;
			}
		}

		auto* package = a_actor->GetCurrentPackage();
		if (!package) {
			return {};
		}
		Destination found{};
		found.package = package->GetFormID();
		found.kind = KindOf(package);
		if (GoesNowhere(package)) {
			found.still = true;
			return found;
		}
		const auto place = PlaceOf(package, a_actor, nullptr);
		if (!place.valid) {
			return {};
		}
		found.valid = true;
		found.point = place.point;
		found.radius = place.radius;
		found.source = Tripod::Source::kPackage;
		found.target = place.target;
		found.how = place.how;
		return found;
	}
}
