#include "SD/Camera/Situational.h"

#include "SD/Camera/Director.h"
#include "SD/Camera/PresetRules.h"
#include "SD/Camera/Presets.h"
#include "SD/Core/Logging.h"
#include "SD/Scene/LightRig.h"

#include <chrono>
#include <map>

namespace SD::Camera
{
	namespace
	{
		namespace PR = PresetRules;

		constexpr std::size_t kShotCount = static_cast<std::size_t>(ShotType::kCount);

		// Everything a preset sets, as it stands in the live settings.
		struct Settings
		{
			Style                                               style{};
			std::array<InstalledPreset::Entry, kShotCount> shots{};
		};

		std::optional<InstalledPreset>        chosen;   // for this conversation; kept for a resume
		std::optional<Settings>               before;   // your own settings, while one is on
		Settings                              applied;  // what was put on
		std::optional<Situational::Person>    lastPerson;

		// Skyrim.esm forms, so their IDs are fixed.
		constexpr RE::FormID kActorTypeNPC = 0x13794;
		constexpr RE::FormID kCurrentFollowerFaction = 0x5C84E;
		constexpr RE::FormID kJobMerchantFaction = 0x51596;
		constexpr RE::FormID kJobInnkeeperFaction = 0x5091B;

		[[nodiscard]] std::uint32_t Roll()
		{
			static std::uint32_t state = static_cast<std::uint32_t>(
				std::chrono::steady_clock::now().time_since_epoch().count() | 1);
			state = state * 1664525u + 1013904223u;
			return (state >> 16) % 100u;
		}

		[[nodiscard]] Settings Live()
		{
			Settings out;
			const auto tunables = Director::GetTunables();
			out.style = { tunables.minShotTime, tunables.maxShotTime, tunables.cutEveryMin, tunables.cutEveryMax,
				tunables.perLineAngleChange, tunables.holdOnShortLines, tunables.timedCutsWhileSpeaking,
				tunables.timedCutsWhileChoosing };
			for (std::size_t i = 0; i < kShotCount; ++i) {
				const auto type = static_cast<ShotType>(i);
				out.shots[i] = { Shot::Enabled(type), Shot::MoveOf(type), Shot::MoveAmount(type), Shot::MoveTime(type),
					Shot::Lens(type), Shot::Weight(type), Shot::LightOf(type) };
			}
			return out;
		}

		// The same writes ApplyInstalledPreset makes, minus the ini.
		void Put(const InstalledPreset& a_preset)
		{
			auto        tunables = Director::GetTunables();
			const auto& s = a_preset.style;
			tunables.minShotTime = s.minShotTime;
			tunables.maxShotTime = s.maxShotTime;
			tunables.cutEveryMin = s.cutEveryMin;
			tunables.cutEveryMax = s.cutEveryMax;
			tunables.perLineAngleChange = s.perLineAngleChange;
			tunables.holdOnShortLines = s.holdOnShortLines;
			tunables.timedCutsWhileSpeaking = s.timedCutsWhileSpeaking;
			tunables.timedCutsWhileChoosing = s.timedCutsWhileChoosing;
			Director::ApplyTunables(tunables);

			const auto looks = Scene::AllLooks().size();
			for (std::size_t i = 0; i < kShotCount; ++i) {
				const auto  type = static_cast<ShotType>(i);
				const auto& entry = a_preset.shots[i];
				Shot::SetEnabled(type, entry.on);
				if (!entry.on) {
					continue;
				}
				Shot::SetMove(type, entry.move);
				Shot::SetMoveAmount(type, entry.amount);
				Shot::SetMoveTime(type, entry.time);
				Shot::SetLens(type, entry.lens);
				Shot::SetWeight(type, entry.weight);
				if (entry.light >= 0 && static_cast<std::size_t>(entry.light) < looks) {
					Shot::SetLight(type, entry.light);
				}
			}
		}

		// Puts your own settings back, except where the live value is no longer what
		// the preset put there: that was changed in the menu and stays.
		void TakeOff()
		{
			if (!before) {
				return;
			}
			const auto& was = *before;
			const auto  back = [](auto& a_live, const auto& a_applied, const auto& a_was) {
				if (a_live == a_applied) {
					a_live = a_was;
				}
			};

			auto tunables = Director::GetTunables();
			back(tunables.minShotTime, applied.style.minShotTime, was.style.minShotTime);
			back(tunables.maxShotTime, applied.style.maxShotTime, was.style.maxShotTime);
			back(tunables.cutEveryMin, applied.style.cutEveryMin, was.style.cutEveryMin);
			back(tunables.cutEveryMax, applied.style.cutEveryMax, was.style.cutEveryMax);
			back(tunables.perLineAngleChange, applied.style.perLineAngleChange, was.style.perLineAngleChange);
			back(tunables.holdOnShortLines, applied.style.holdOnShortLines, was.style.holdOnShortLines);
			back(tunables.timedCutsWhileSpeaking, applied.style.timedCutsWhileSpeaking, was.style.timedCutsWhileSpeaking);
			back(tunables.timedCutsWhileChoosing, applied.style.timedCutsWhileChoosing, was.style.timedCutsWhileChoosing);
			Director::ApplyTunables(tunables);

			const auto live = Live();
			for (std::size_t i = 0; i < kShotCount; ++i) {
				const auto  type = static_cast<ShotType>(i);
				auto        now = live.shots[i];
				const auto& put = applied.shots[i];
				const auto& old = was.shots[i];
				back(now.on, put.on, old.on);
				back(now.move, put.move, old.move);
				back(now.amount, put.amount, old.amount);
				back(now.time, put.time, old.time);
				back(now.lens, put.lens, old.lens);
				back(now.weight, put.weight, old.weight);
				back(now.light, put.light, old.light);
				Shot::SetEnabled(type, now.on);
				Shot::SetMove(type, now.move);
				Shot::SetMoveAmount(type, now.amount);
				Shot::SetMoveTime(type, now.time);
				Shot::SetLens(type, now.lens);
				Shot::SetWeight(type, now.weight);
				Shot::SetLight(type, now.light);
			}
			before.reset();
		}

		void PutOn()
		{
			if (!chosen || before) {
				return;
			}
			before = Live();
			Put(*chosen);
			applied = Live();
		}

		// ---- What the conditions can see ----------------------------------------

		[[nodiscard]] std::string Text(const char* a_text)
		{
			return a_text ? std::string{ a_text } : std::string{};
		}

		void AddName(std::vector<std::string>& a_out, const char* a_text)
		{
			if (a_text && *a_text) {
				a_out.emplace_back(a_text);
			}
		}

		[[nodiscard]] std::optional<PR::Form> FormOf(const RE::TESForm* a_form)
		{
			const auto* file = a_form ? a_form->GetFile(0) : nullptr;
			if (!file) {
				return std::nullopt;
			}
			const bool light = file->IsLight();
			return PR::Form{ PR::Lower(file->GetFilename()), a_form->GetFormID() & (light ? 0xFFFu : 0xFFFFFFu), light };
		}

		void Add(PR::Situation::Named& a_out, const RE::TESForm* a_form)
		{
			if (!a_form) {
				return;
			}
			AddName(a_out.names, a_form->GetName());
			AddName(a_out.names, a_form->GetFormEditorID());
			if (const auto form = FormOf(a_form)) {
				a_out.forms.push_back(*form);
			}
		}

		void AddKeywords(std::vector<std::string>& a_out, const RE::BGSKeywordForm* a_form)
		{
			if (!a_form || !a_form->keywords) {
				return;
			}
			for (std::uint32_t i = 0; i < a_form->numKeywords; ++i) {
				if (const auto* keyword = a_form->keywords[i]) {
					AddName(a_out, keyword->GetFormEditorID());
				}
			}
		}

		[[nodiscard]] PR::Situation Gather(RE::Actor* a_partner, bool a_overheard)
		{
			PR::Situation out;
			out.overheard = a_overheard;

			if (auto* calendar = RE::Calendar::GetSingleton()) {
				out.hour = calendar->GetHour();
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (player) {
				const auto* cell = player->GetParentCell();
				out.inside = cell && cell->IsInteriorCell();

				int depth = 0;
				for (auto* location = player->GetCurrentLocation(); location && depth < 16;
					 location = location->parentLoc, ++depth) {
					Add(out.location, location);
					AddKeywords(out.locationKeywords, location);
				}
			}

			// Indoors the sky's weather is the last one outside, which no one in the
			// room can see.
			if (!out.inside) {
				if (auto* sky = RE::Sky::GetSingleton(); sky && sky->currentWeather) {
					using Flag = RE::TESWeather::WeatherDataFlag;
					const auto& flags = sky->currentWeather->data.flags;
					if (flags.any(Flag::kPleasant)) {
						out.weather.emplace_back("clear");
					}
					if (flags.any(Flag::kCloudy)) {
						out.weather.emplace_back("cloudy");
					}
					if (flags.any(Flag::kRainy)) {
						out.weather.emplace_back("rain");
					}
					if (flags.any(Flag::kSnow)) {
						out.weather.emplace_back("snow");
					}
				}
			}

			if (!a_partner) {
				return out;
			}

			Add(out.npc, a_partner);
			auto* base = a_partner->GetActorBase();
			Add(out.npc, base);
			if (a_partner->data.objectReference != base) {
				Add(out.npc, a_partner->data.objectReference);
			}

			auto* race = a_partner->GetRace();
			Add(out.race, race);
			AddKeywords(out.keywords, base);
			AddKeywords(out.keywords, race);

			// Base factions first, then changes made in play; the last rank wins, and a
			// negative one means they've left.
			std::map<RE::TESFaction*, std::int8_t> ranks;
			a_partner->VisitFactions([&ranks](RE::TESFaction* a_faction, std::int8_t a_rank) {
				if (a_faction) {
					ranks[a_faction] = a_rank;
				}
				return false;
			});
			bool vendor = false;
			for (const auto& [faction, rank] : ranks) {
				if (rank < 0) {
					continue;
				}
				Add(out.factions, faction);
				vendor |= faction->IsVendor();
			}
			const auto member = [&ranks](RE::FormID a_id) {
				const auto it = std::ranges::find_if(ranks, [a_id](const auto& a_entry) {
					return a_entry.first->GetFormID() == a_id;
				});
				return it != ranks.end() && it->second >= 0;
			};

			if (a_partner->IsPlayerTeammate() || member(kCurrentFollowerFaction)) {
				out.who.emplace_back("follower");
			}
			if (vendor || member(kJobMerchantFaction)) {
				out.who.emplace_back("merchant");
			}
			if (member(kJobInnkeeperFaction)) {
				out.who.emplace_back("innkeeper");
			}
			if (a_partner->IsGuard()) {
				out.who.emplace_back("guard");
			}
			if (a_partner->IsChild()) {
				out.who.emplace_back("child");
			}
			if (auto* npcKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(kActorTypeNPC);
				npcKeyword && !a_partner->HasKeyword(npcKeyword)) {
				out.who.emplace_back("creature");
			}
			return out;
		}

		void Remember(RE::Actor* a_partner)
		{
			if (!a_partner) {
				return;
			}
			Situational::Person person;
			person.name = Text(a_partner->GetName());

			// The placed reference names this one person; a spawned one has no file, so
			// fall back to their base.
			for (const RE::TESForm* form : { static_cast<const RE::TESForm*>(a_partner),
					 static_cast<const RE::TESForm*>(a_partner->GetActorBase()) }) {
				if (const auto* file = form ? form->GetFile(0) : nullptr) {
					const auto id = FormOf(form);
					person.form = PR::FormText(file->GetFilename(), id ? id->id : 0);
					break;
				}
			}
			lastPerson = std::move(person);
		}
	}

	void Situational::Begin(RE::Actor* a_partner, bool a_overheard)
	{
		TakeOff();
		chosen.reset();
		Remember(a_partner);

		if (!Director::GetTunables().situationalPresets) {
			return;
		}

		RescanPresetFiles(false);
		const auto& installed = InstalledPresets();
		if (std::ranges::none_of(installed, [](const InstalledPreset& a_preset) { return a_preset.usable && a_preset.when; })) {
			return;
		}

		const auto situation = Gather(a_partner, a_overheard);

		// Highest priority, then the most specific; the list is sorted by name, so a
		// tie goes to the first.
		const InstalledPreset* best = nullptr;
		for (const auto& preset : installed) {
			if (!preset.usable || !preset.when || !PR::Holds(*preset.when, situation)) {
				continue;
			}
			if (preset.unless && PR::Holds(*preset.unless, situation)) {
				continue;
			}
			if (preset.when->chance && static_cast<int>(Roll()) >= *preset.when->chance) {
				Log::Info(Log::Category::kCamera, "Preset '{}' fits this conversation but lost its {}% roll."sv,
					preset.name, *preset.when->chance);
				continue;
			}
			if (!best || preset.priority > best->priority ||
				(preset.priority == best->priority && preset.when->Count() > best->when->Count())) {
				best = &preset;
			}
		}

		if (!best) {
			Log::Info(Log::Category::kCamera, "No preset is set for this situation; using your own settings."sv);
			return;
		}

		chosen = *best;
		PutOn();
		Log::Info(Log::Category::kCamera, "Preset '{}' ({}) for this conversation: {}."sv, chosen->name, chosen->file,
			PR::Describe(*chosen->when, NameOf));
	}

	void Situational::Resume()
	{
		PutOn();
	}

	void Situational::End()
	{
		TakeOff();
	}

	std::string Situational::Current()
	{
		return before && chosen ? chosen->name : std::string{};
	}

	std::optional<Situational::Person> Situational::LastPerson()
	{
		return lastPerson;
	}

	std::string Situational::NameOf(std::string_view a_form)
	{
		const auto form = PR::ParseForm(a_form);
		auto*      data = RE::TESDataHandler::GetSingleton();
		if (!form || !data) {
			return {};
		}
		const auto* file = data->LookupModByName(form->plugin);
		if (!file) {
			return {};
		}
		const auto  local = form->id & (file->IsLight() ? 0xFFFu : 0xFFFFFFu);
		const auto* found = data->LookupForm(local, file->GetFilename());
		return found ? Text(found->GetName()) : std::string{};
	}
}
