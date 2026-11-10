#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace SD::Camera::PresetRules
{
	// When a preset is used on its own. A preset file can say when it applies:
	//
	//   "when":   { "location": ["inn", "tavern"], "time": "night" },
	//   "unless": { "who": "follower" },
	//   "priority": 1
	//
	// Every condition in a block has to hold, and a list holds if any of its
	// entries does. As each conversation starts, every installed preset whose
	// "when" holds and whose "unless" doesn't is a candidate: the highest priority
	// wins, then the one with the most conditions, then the first by name. It lasts
	// for that conversation, and your own settings come back afterwards.
	//
	// No game types here, so it can be unit tested. The game fills in a Situation.

	[[nodiscard]] inline std::string Lower(std::string_view a_text)
	{
		std::string out{ a_text };
		for (auto& c : out) {
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			}
		}
		return out;
	}

	// "Push in", "pushIn" and "push_in" are the same name.
	[[nodiscard]] inline std::string Simplify(std::string_view a_text)
	{
		std::string out;
		for (const char c : a_text) {
			if (c == ' ' || c == '-' || c == '_' || c == '\t') {
				continue;
			}
			out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
		}
		return out;
	}

	// A form named by its plugin and ID: "Skyrim.esm|A2C94" or "0xA2C94~Skyrim.esm".
	struct Form
	{
		std::string   plugin;  // lower case
		std::uint32_t id{ 0 };
		bool          light{ false };  // only meaningful for forms the game reports

		bool operator==(const Form&) const = default;
	};

	// Hours of the day, from inclusive to exclusive, wrapping past midnight.
	struct Hours
	{
		float       from{ 0.0f };
		float       to{ 24.0f };
		std::string text;  // as it's written back

		[[nodiscard]] bool Contains(float a_hour) const
		{
			return from <= to ? a_hour >= from && a_hour < to : a_hour >= from || a_hour < to;
		}

		bool operator==(const Hours& a_other) const { return text == a_other.text; }
	};

	struct Rules
	{
		std::vector<std::string> who;       // the kinds in kWho
		std::vector<std::string> npc;       // names or forms
		std::vector<std::string> race;      // names, editor IDs or forms
		std::vector<std::string> faction;   // names or forms
		std::vector<std::string> keyword;   // editor IDs on them or their race
		std::vector<std::string> location;  // a kind of place, a location's name, a keyword or a form
		std::vector<std::string> weather;   // clear, cloudy, rain, snow
		std::vector<Hours>       time;
		std::optional<bool>      inside;
		std::optional<bool>      overheard;  // "conversation": filming other people, or your own
		std::optional<int>       chance;     // percent of the conversations it would apply to

		// How specific the block is. Chance doesn't count; it isn't a condition on
		// the situation.
		[[nodiscard]] int Count() const
		{
			const auto has = [](const auto& a_list) { return a_list.empty() ? 0 : 1; };
			return has(who) + has(npc) + has(race) + has(faction) + has(keyword) + has(location) + has(weather) +
			       has(time) + (inside ? 1 : 0) + (overheard ? 1 : 0);
		}

		[[nodiscard]] bool Empty() const { return Count() == 0 && !chance; }

		bool operator==(const Rules&) const = default;
	};

	// Everything the conditions can ask about one conversation.
	struct Situation
	{
		// A thing a condition can name: by any of its names (display name, editor
		// ID), or by plugin and ID.
		struct Named
		{
			std::vector<std::string> names;
			std::vector<Form>        forms;
		};

		bool  inside{ false };
		float hour{ 12.0f };
		bool  overheard{ false };

		std::vector<std::string> weather;   // canonical words; empty indoors
		std::vector<std::string> who;       // the kinds in kWho that apply to them
		Named                    npc;       // the actor, their base and what was placed
		Named                    race;
		Named                    factions;  // every faction they're in
		std::vector<std::string> keywords;  // editor IDs on them and their race
		Named                    location;  // where they are and everywhere that's inside of
		std::vector<std::string> locationKeywords;
	};

	// The kinds of people "who" can name. The first spelling is the one written
	// out; the others are accepted.
	inline constexpr std::array<std::array<std::string_view, 4>, 6> kWho{ {
		{ "follower", "followers", "companion", "companions" },
		{ "merchant", "merchants", "vendor", "trader" },
		{ "innkeeper", "innkeepers", "barkeep", "tavernkeeper" },
		{ "guard", "guards", "", "" },
		{ "child", "children", "kid", "kids" },
		{ "creature", "creatures", "animal", "animals" },
	} };

	inline constexpr std::array<std::array<std::string_view, 3>, 4> kWeather{ {
		{ "clear", "sunny", "pleasant" },
		{ "cloudy", "overcast", "clouds" },
		{ "rain", "rainy", "raining" },
		{ "snow", "snowy", "snowing" },
	} };

	// Kinds of place for "location", and the location keywords that mark them.
	struct PlaceKind
	{
		std::string_view                 word;
		std::string_view                 alias;
		std::array<std::string_view, 3> keywords;
	};

	inline constexpr std::array<PlaceKind, 17> kPlaces{ {
		{ "inn", "tavern", { "LocTypeInn" } },
		{ "city", "", { "LocTypeCity" } },
		{ "town", "", { "LocTypeTown" } },
		{ "settlement", "village", { "LocTypeSettlement" } },
		{ "home", "house", { "LocTypeHouse", "LocTypePlayerHouse" } },
		{ "dungeon", "", { "LocTypeDungeon" } },
		{ "castle", "keep", { "LocTypeCastle" } },
		{ "temple", "", { "LocTypeTemple" } },
		{ "shop", "store", { "LocTypeStore" } },
		{ "jail", "prison", { "LocTypeJail" } },
		{ "guild", "", { "LocTypeGuild" } },
		{ "farm", "", { "LocTypeFarm" } },
		{ "mine", "", { "LocTypeMine" } },
		{ "camp", "", { "LocTypeMilitaryCamp", "LocTypeBanditCamp", "LocTypeForswornCamp" } },
		{ "barracks", "", { "LocTypeBarracks" } },
		{ "cemetery", "graveyard", { "LocTypeCemetery" } },
		{ "ship", "", { "LocTypeShip" } },
	} };

	struct NamedHours
	{
		std::string_view word;
		float            from;
		float            to;
	};

	inline constexpr std::array<NamedHours, 5> kTimes{ {
		{ "day", 6.0f, 20.0f },
		{ "night", 20.0f, 6.0f },
		{ "morning", 6.0f, 12.0f },
		{ "afternoon", 12.0f, 17.0f },
		{ "evening", 17.0f, 21.0f },
	} };

	// "Skyrim.esm|A2C94", "Skyrim.esm|0x000A2C94" or "0xA2C94~Skyrim.esm". Anything
	// else isn't a form.
	[[nodiscard]] inline std::optional<Form> ParseForm(std::string_view a_text)
	{
		const auto trim = [](std::string_view a_part) {
			while (!a_part.empty() && (a_part.front() == ' ' || a_part.front() == '\t')) {
				a_part.remove_prefix(1);
			}
			while (!a_part.empty() && (a_part.back() == ' ' || a_part.back() == '\t')) {
				a_part.remove_suffix(1);
			}
			return a_part;
		};
		const auto isPlugin = [](std::string_view a_part) {
			const auto lower = Lower(a_part);
			return lower.size() > 4 &&
			       (lower.ends_with(".esp") || lower.ends_with(".esm") || lower.ends_with(".esl"));
		};

		const auto split = a_text.find_first_of("|~");
		if (split == std::string_view::npos) {
			return std::nullopt;
		}
		const auto left = trim(a_text.substr(0, split));
		const auto right = trim(a_text.substr(split + 1));

		std::string_view plugin;
		std::string_view id;
		if (isPlugin(left)) {
			plugin = left;
			id = right;
		} else if (isPlugin(right)) {
			plugin = right;
			id = left;
		} else {
			return std::nullopt;
		}

		if (id.size() > 2 && id[0] == '0' && (id[1] == 'x' || id[1] == 'X')) {
			id.remove_prefix(2);
		}
		if (id.empty() || id.size() > 8) {
			return std::nullopt;
		}
		std::uint32_t value = 0;
		for (const char c : id) {
			std::uint32_t digit = 0;
			if (c >= '0' && c <= '9') {
				digit = static_cast<std::uint32_t>(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				digit = static_cast<std::uint32_t>(c - 'a' + 10);
			} else if (c >= 'A' && c <= 'F') {
				digit = static_cast<std::uint32_t>(c - 'A' + 10);
			} else {
				return std::nullopt;
			}
			value = value * 16 + digit;
		}
		return Form{ Lower(plugin), value, false };
	}

	// How the game's own form is written in a file.
	[[nodiscard]] inline std::string FormText(std::string_view a_plugin, std::uint32_t a_id)
	{
		char buffer[16]{};
		std::snprintf(buffer, sizeof(buffer), "%X", a_id);
		return std::string{ a_plugin } + "|" + buffer;
	}

	// A written form against one the game reports. The load order byte people copy
	// out of xEdit is ignored.
	[[nodiscard]] inline bool SameForm(const Form& a_written, const Form& a_actual)
	{
		if (a_written.plugin != a_actual.plugin) {
			return false;
		}
		const std::uint32_t mask = a_actual.light ? 0xFFFu : 0xFFFFFFu;
		return (a_written.id & mask) == (a_actual.id & mask);
	}

	[[nodiscard]] inline bool AnyName(const std::vector<std::string>& a_names, std::string_view a_entry)
	{
		const auto wanted = Simplify(a_entry);
		return !wanted.empty() &&
		       std::ranges::any_of(a_names, [&wanted](const std::string& a_name) { return Simplify(a_name) == wanted; });
	}

	[[nodiscard]] inline bool Names(const Situation::Named& a_thing, std::string_view a_entry)
	{
		if (const auto form = ParseForm(a_entry)) {
			return std::ranges::any_of(a_thing.forms, [&form](const Form& a_form) { return SameForm(*form, a_form); });
		}
		return AnyName(a_thing.names, a_entry);
	}

	[[nodiscard]] inline const PlaceKind* FindPlace(std::string_view a_entry)
	{
		const auto wanted = Simplify(a_entry);
		for (const auto& place : kPlaces) {
			if (Simplify(place.word) == wanted || (!place.alias.empty() && Simplify(place.alias) == wanted)) {
				return &place;
			}
		}
		return nullptr;
	}

	[[nodiscard]] inline bool AtPlace(const Situation& a_situation, std::string_view a_entry)
	{
		if (const auto* place = FindPlace(a_entry)) {
			return std::ranges::any_of(place->keywords, [&a_situation](std::string_view a_keyword) {
				return !a_keyword.empty() && AnyName(a_situation.locationKeywords, a_keyword);
			});
		}
		return Names(a_situation.location, a_entry) || AnyName(a_situation.locationKeywords, a_entry);
	}

	// Whether every condition in the block holds. Chance is the caller's to roll.
	[[nodiscard]] inline bool Holds(const Rules& a_rules, const Situation& a_situation)
	{
		const auto any = [](const std::vector<std::string>& a_entries, const auto& a_test) {
			return a_entries.empty() || std::ranges::any_of(a_entries, a_test);
		};

		if (a_rules.inside && *a_rules.inside != a_situation.inside) {
			return false;
		}
		if (a_rules.overheard && *a_rules.overheard != a_situation.overheard) {
			return false;
		}
		if (!a_rules.time.empty() && std::ranges::none_of(a_rules.time, [&a_situation](const Hours& a_hours) {
				return a_hours.Contains(a_situation.hour);
			})) {
			return false;
		}
		return any(a_rules.weather, [&](const std::string& a_entry) { return AnyName(a_situation.weather, a_entry); }) &&
		       any(a_rules.who, [&](const std::string& a_entry) { return AnyName(a_situation.who, a_entry); }) &&
		       any(a_rules.npc, [&](const std::string& a_entry) { return Names(a_situation.npc, a_entry); }) &&
		       any(a_rules.race, [&](const std::string& a_entry) { return Names(a_situation.race, a_entry); }) &&
		       any(a_rules.faction, [&](const std::string& a_entry) { return Names(a_situation.factions, a_entry); }) &&
		       any(a_rules.keyword, [&](const std::string& a_entry) { return AnyName(a_situation.keywords, a_entry); }) &&
		       any(a_rules.location, [&](const std::string& a_entry) { return AtPlace(a_situation, a_entry); });
	}

	namespace detail
	{
		using Json = nlohmann::ordered_json;

		// "7", "7:30" or "19.5" as hours, or nothing.
		[[nodiscard]] inline std::optional<float> Clock(std::string_view a_text)
		{
			while (!a_text.empty() && a_text.front() == ' ') {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && a_text.back() == ' ') {
				a_text.remove_suffix(1);
			}
			if (a_text.empty() || a_text.size() > 8) {
				return std::nullopt;
			}
			float hours = 0.0f;
			float minutes = 0.0f;
			bool  inMinutes = false;
			int   digits = 0;
			for (const char c : a_text) {
				if (c == ':' && !inMinutes && digits > 0) {
					inMinutes = true;
					digits = 0;
				} else if (c >= '0' && c <= '9') {
					auto& field = inMinutes ? minutes : hours;
					field = field * 10.0f + static_cast<float>(c - '0');
					++digits;
				} else {
					return std::nullopt;
				}
			}
			if (digits == 0 || hours > 24.0f || minutes >= 60.0f) {
				return std::nullopt;
			}
			return std::fmod(hours + minutes / 60.0f, 24.0f);
		}

		[[nodiscard]] inline std::optional<Hours> ReadHours(std::string_view a_text)
		{
			const auto wanted = Simplify(a_text);
			for (const auto& named : kTimes) {
				if (named.word == wanted) {
					return Hours{ named.from, named.to, std::string{ named.word } };
				}
			}
			const auto dash = a_text.find('-');
			if (dash == std::string_view::npos) {
				return std::nullopt;
			}
			const auto from = Clock(a_text.substr(0, dash));
			const auto to = Clock(a_text.substr(dash + 1));
			if (!from || !to || *from == *to) {
				return std::nullopt;
			}
			std::string text{ a_text };
			std::erase(text, ' ');
			return Hours{ *from, *to, text };
		}

		// The canonical spelling from a table of alternatives, or nothing.
		template <std::size_t N, std::size_t M>
		[[nodiscard]] std::optional<std::string> Canonical(const std::array<std::array<std::string_view, M>, N>& a_table,
			std::string_view a_entry)
		{
			const auto wanted = Simplify(a_entry);
			for (const auto& spellings : a_table) {
				for (const auto spelling : spellings) {
					if (!spelling.empty() && Simplify(spelling) == wanted) {
						return std::string{ spellings.front() };
					}
				}
			}
			return std::nullopt;
		}

		// A string or a list of strings. Anything else in a list is reported.
		[[nodiscard]] inline std::vector<std::string> Entries(const Json& a_value, std::string_view a_where,
			std::vector<std::string>& a_warnings)
		{
			std::vector<std::string> out;
			const auto take = [&](const Json& a_entry) {
				if (a_entry.is_string() && !a_entry.get<std::string>().empty()) {
					out.push_back(a_entry.get<std::string>());
				} else {
					a_warnings.push_back(std::string{ a_where } + " should be text, or a list of text; part ignored.");
				}
			};
			if (a_value.is_array()) {
				for (const auto& entry : a_value) {
					take(entry);
				}
			} else {
				take(a_value);
			}
			return out;
		}
	}

	// Reads a "when" or "unless" block. a_block is its name, for the warnings.
	inline void Read(const nlohmann::ordered_json& a_object, Rules& a_out, std::string_view a_block,
		std::vector<std::string>& a_warnings)
	{
		using detail::Json;

		if (!a_object.is_object()) {
			a_warnings.push_back("\"" + std::string{ a_block } + "\" should be a { ... } block; ignored.");
			return;
		}

		for (const auto& item : a_object.items()) {
			const std::string key = item.key();
			const Json&       value = item.value();
			const auto        id = Simplify(key);
			const auto        where = "\"" + key + "\" in " + std::string{ a_block };

			const auto flag = [&](std::optional<bool>& a_field, bool a_invert) {
				if (value.is_boolean()) {
					a_field = value.get<bool>() != a_invert;
				} else {
					a_warnings.push_back(where + " should be true or false; ignored.");
				}
			};
			const auto words = [&](std::vector<std::string>& a_field, const auto& a_table, std::string_view a_choices) {
				for (const auto& entry : detail::Entries(value, where, a_warnings)) {
					if (auto canonical = detail::Canonical(a_table, entry)) {
						a_field.push_back(std::move(*canonical));
					} else {
						a_warnings.push_back(where + ": \"" + entry + "\" isn't one of " + std::string{ a_choices } + "; ignored.");
					}
				}
			};
			const auto text = [&](std::vector<std::string>& a_field) {
				for (auto& entry : detail::Entries(value, where, a_warnings)) {
					a_field.push_back(std::move(entry));
				}
			};

			if (id == "who") {
				words(a_out.who, kWho, "follower, merchant, innkeeper, guard, child, creature");
			} else if (id == "npc" || id == "person" || id == "name") {
				text(a_out.npc);
			} else if (id == "race") {
				text(a_out.race);
			} else if (id == "faction") {
				text(a_out.faction);
			} else if (id == "keyword") {
				text(a_out.keyword);
			} else if (id == "location" || id == "place") {
				text(a_out.location);
			} else if (id == "weather") {
				words(a_out.weather, kWeather, "clear, cloudy, rain, snow");
			} else if (id == "time") {
				for (const auto& entry : detail::Entries(value, where, a_warnings)) {
					if (auto hours = detail::ReadHours(entry)) {
						a_out.time.push_back(std::move(*hours));
					} else {
						a_warnings.push_back(where + ": \"" + entry +
											 "\" isn't day, night, morning, afternoon, evening or hours like 20-6; ignored.");
					}
				}
			} else if (id == "inside" || id == "indoors") {
				flag(a_out.inside, false);
			} else if (id == "outside" || id == "outdoors") {
				flag(a_out.inside, true);
			} else if (id == "conversation") {
				const auto word = value.is_string() ? Simplify(value.get<std::string>()) : std::string{};
				if (word == "yours" || word == "mine" || word == "player") {
					a_out.overheard = false;
				} else if (word == "overheard" || word == "others" || word == "theirs" || word == "scene") {
					a_out.overheard = true;
				} else {
					a_warnings.push_back(where + " should be \"yours\" or \"overheard\"; ignored.");
				}
			} else if (id == "chance") {
				if (value.is_number()) {
					a_out.chance = static_cast<int>(std::lround(std::clamp(value.get<double>(), 0.0, 100.0)));
				} else {
					a_warnings.push_back(where + " should be a percentage; ignored.");
				}
			} else {
				a_warnings.push_back(where + " isn't a condition; ignored.");
			}
		}
	}

	// One block on one line, for a file.
	[[nodiscard]] inline std::string Write(const Rules& a_rules)
	{
		using detail::Json;

		std::vector<std::string> parts;
		const auto quote = [](std::string_view a_text) {
			return Json(std::string{ a_text }).dump(-1, ' ', false, Json::error_handler_t::replace);
		};
		const auto list = [&](const char* a_key, const std::vector<std::string>& a_entries) {
			if (a_entries.empty()) {
				return;
			}
			std::string body;
			for (std::size_t i = 0; i < a_entries.size(); ++i) {
				body += (i == 0 ? "" : ", ") + quote(a_entries[i]);
			}
			parts.push_back("\"" + std::string{ a_key } + "\": " + (a_entries.size() == 1 ? body : "[" + body + "]"));
		};

		if (a_rules.inside) {
			parts.push_back(std::string{ "\"inside\": " } + (*a_rules.inside ? "true" : "false"));
		}
		list("location", a_rules.location);
		if (!a_rules.time.empty()) {
			std::vector<std::string> times;
			for (const auto& hours : a_rules.time) {
				times.push_back(hours.text);
			}
			list("time", times);
		}
		list("weather", a_rules.weather);
		list("who", a_rules.who);
		list("npc", a_rules.npc);
		list("race", a_rules.race);
		list("faction", a_rules.faction);
		list("keyword", a_rules.keyword);
		if (a_rules.overheard) {
			parts.push_back(std::string{ "\"conversation\": " } + (*a_rules.overheard ? "\"overheard\"" : "\"yours\""));
		}
		if (a_rules.chance) {
			parts.push_back("\"chance\": " + std::to_string(*a_rules.chance));
		}

		std::string out = "{";
		for (std::size_t i = 0; i < parts.size(); ++i) {
			out += (i == 0 ? " " : ", ") + parts[i];
		}
		return out + (parts.empty() ? "}" : " }");
	}

	// One block as a short phrase for the menu. a_nameOf turns a form into a name
	// where the game knows one.
	[[nodiscard]] inline std::string Describe(const Rules& a_rules,
		const std::function<std::string(std::string_view)>& a_nameOf = {})
	{
		std::vector<std::string> parts;
		const auto list = [&](const std::vector<std::string>& a_entries, bool a_forms) {
			if (a_entries.empty()) {
				return;
			}
			std::string body;
			for (std::size_t i = 0; i < a_entries.size(); ++i) {
				auto entry = a_entries[i];
				if (a_forms && a_nameOf && ParseForm(entry)) {
					if (auto name = a_nameOf(entry); !name.empty()) {
						entry = std::move(name);
					}
				}
				body += (i == 0 ? "" : " or ") + entry;
			}
			parts.push_back(std::move(body));
		};

		if (a_rules.inside) {
			parts.emplace_back(*a_rules.inside ? "indoors" : "outdoors");
		}
		list(a_rules.location, true);
		if (!a_rules.time.empty()) {
			std::vector<std::string> times;
			for (const auto& hours : a_rules.time) {
				times.push_back(hours.text);
			}
			list(times, false);
		}
		list(a_rules.weather, false);
		list(a_rules.who, false);
		list(a_rules.npc, true);
		list(a_rules.race, true);
		list(a_rules.faction, true);
		list(a_rules.keyword, false);
		if (a_rules.overheard) {
			parts.emplace_back(*a_rules.overheard ? "overheard" : "your conversations");
		}
		if (a_rules.chance) {
			parts.push_back(std::to_string(*a_rules.chance) + "% of the time");
		}

		std::string out;
		for (std::size_t i = 0; i < parts.size(); ++i) {
			out += (i == 0 ? "" : ", ") + parts[i];
		}
		return out.empty() ? "always" : out;
	}
}
