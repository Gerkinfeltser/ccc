#include "SD/Camera/PresetFile.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Expect(bool condition, const char* message)
	{
		if (!condition) {
			std::cerr << "PresetFile: " << message << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	namespace PF = SD::Camera::PresetFile;

	const PF::Vocabulary kWords{
		{ { "OverPlayerShoulder" }, { "CloseUp" }, { "ClosePlayer" }, { "Distant" } },
		{ { "None", "Locked off" }, { "Push in", "PushIn" }, { "Slide left", "TruckLeft" } },
		{ { "off", "Off" }, { "soft", "Soft" }, { "edge", "Edge only" } },
	};

	[[nodiscard]] const PF::Angle* AngleIn(const PF::Preset& a_preset, int a_index)
	{
		for (const auto& angle : a_preset.angles) {
			if (angle.angle == a_index) {
				return &angle;
			}
		}
		return nullptr;
	}
}

int main()
{
	// A typical hand-written file: BOM, comments, loose spelling.
	{
		const auto file = PF::Read(
			"\xEF\xBB\xBF{\n"
			"  // my look\n"
			"  \"name\": \"Tavern Talk\",\n"
			"  \"Author\": \"Someone\",\n"
			"  \"cutting\": { \"minimumLines\": 3, \"maximum lines\": 6, \"shortestHold\": 2.4,\n"
			"               \"changeAfter\": 9, \"ignoreShortLines\": false, \"timerWhileTalking\": 1 },\n"
			"  \"angles\": {\n"
			"    \"closeup\": { \"frequency\": 60, \"fov\": 44.6, \"effect\": \"push-in\", \"amount\": 40, \"duration\": 8.0 },\n"
			"    \"Close Player\": {},\n"
			"    \"Distant\": { \"light\": \"Edge only\", \"effect\": \"truck_left\" }\n"
			"  }\n"
			"}\n",
			kWords);

		Expect(file.error.empty(), "a valid file reads");
		Expect(file.presets.size() == 1, "one object is one preset");
		const auto& preset = file.presets.front();
		Expect(preset.warnings.empty(), "nothing in a valid file is skipped");
		Expect(preset.name == "Tavern Talk" && preset.author == "Someone", "text fields read, keys ignore case");
		Expect(preset.cutting.minimumLines == 3 && preset.cutting.maximumLines == 6, "line counts read, spaces in keys ignored");
		Expect(preset.cutting.shortestHold == 240 && preset.cutting.changeAfter == 900, "seconds become hundredths");
		Expect(preset.cutting.ignoreShortLines == false && preset.cutting.timerWhileTalking == true, "switches read, 1 is true");
		Expect(!preset.cutting.changeAnglePerLine && !preset.cutting.timerWhileChoosing, "absent settings stay empty");

		Expect(preset.angles.size() == 3, "listed angles are the ones in use");
		Expect(!AngleIn(preset, 0), "unlisted angles are off");
		const auto* closeUp = AngleIn(preset, 1);
		Expect(closeUp && closeUp->frequency == 60 && closeUp->fov == 45, "numbers read and round");
		Expect(closeUp->effect == 1 && closeUp->amount == 40 && closeUp->duration == 800, "effects match loosely, durations in seconds");
		const auto* closePlayer = AngleIn(preset, 2);
		Expect(closePlayer && !closePlayer->frequency && !closePlayer->effect && !closePlayer->light, "an empty block keeps every default");
		const auto* distant = AngleIn(preset, 3);
		Expect(distant && distant->light == 2 && distant->effect == 2, "lights and effects match any of their names");
	}

	// Mistakes are skipped with a reason, not fatal.
	{
		const auto file = PF::Read(
			"{ \"name\": 5, \"colour\": \"red\", \"cutting\": { \"minimumLines\": \"three\", \"speed\": 1 },\n"
			"  \"angles\": { \"CloseUp\": { \"wieght\": 5, \"effect\": \"Barrel roll\" }, \"Sideways\": {}, \"Distant\": 7 } }",
			kWords);
		Expect(file.error.empty() && file.presets.size() == 1, "a file with mistakes still reads");
		const auto& preset = file.presets.front();
		Expect(preset.warnings.size() == 8, "each mistake is reported once");
		Expect(preset.name.empty(), "a name that isn't text is left empty");
		Expect(!preset.cutting.minimumLines, "a number written as a word is skipped");
		Expect(AngleIn(preset, 1) && !AngleIn(preset, 1)->effect, "an unknown effect is skipped");
		Expect(!AngleIn(preset, 0) && AngleIn(preset, 3), "an unknown angle is skipped; a bad block still turns its angle on");
	}

	// Several presets in one file.
	{
		const auto file = PF::Read("[ { \"name\": \"A\" }, 3, { \"name\": \"B\" } ]", kWords);
		Expect(file.error.empty() && file.presets.size() == 2, "a list holds several presets");
		Expect(file.presets[0].name == "A" && file.presets[1].name == "B", "in order");
		Expect(file.warnings.size() == 1, "a list entry that isn't a preset is reported");
	}

	// Files that can't be read at all.
	{
		const auto broken = PF::Read("{ \"name\": \"A\",, }", kWords);
		Expect(!broken.error.empty() && broken.presets.empty(), "a syntax error is reported");
		Expect(broken.error.find("line 1") != std::string::npos, "with where it is");
		Expect(broken.error.front() != '[', "without the library's prefix");
		Expect(!PF::Read("42", kWords).error.empty(), "a bare value isn't a preset");
		Expect(!PF::Read("[]", kWords).error.empty(), "an empty list has nothing to offer");
		Expect(!PF::Read("", kWords).error.empty(), "an empty file is an error");
	}

	// Written files read back the same.
	{
		PF::Preset preset;
		preset.name = "Quote \" and \\ backslash";
		preset.author = std::string{ "bad \xFF byte" };
		preset.description = "Line";
		preset.cutting.changeAnglePerLine = true;
		preset.cutting.minimumLines = 2;
		preset.cutting.maximumLines = 5;
		preset.cutting.ignoreShortLines = true;
		preset.cutting.timerWhileTalking = false;
		preset.cutting.timerWhileChoosing = false;
		preset.cutting.changeAfter = 905;
		preset.cutting.shortestHold = 240;

		PF::Angle closeUp{};
		closeUp.angle = 1;
		closeUp.frequency = 36;
		closeUp.fov = 50;
		closeUp.effect = 1;
		closeUp.amount = 40;
		closeUp.duration = 800;
		closeUp.light = 1;
		PF::Angle distant{};
		distant.angle = 3;
		distant.effect = 0;
		preset.angles = { closeUp, distant };

		const auto text = PF::Write(preset, kWords);
		Expect(text.find("\"shortestHold\": 2.4") != std::string::npos, "seconds are written short");
		Expect(text.find("\"changeAfter\": 9.05") != std::string::npos, "hundredths survive");
		Expect(text.find("\"effect\": \"Push in\"") != std::string::npos, "effects are written by their menu name");

		const auto back = PF::Read(text, kWords);
		Expect(back.error.empty() && back.presets.size() == 1, "a written file reads");
		const auto& again = back.presets.front();
		Expect(again.warnings.empty(), "a written file has no mistakes");
		Expect(again.name == preset.name && again.description == preset.description, "text survives escaping");
		Expect(again.author.find("bad") == 0, "bytes that aren't UTF-8 don't break the file");
		Expect(again.cutting == preset.cutting, "cutting survives");
		Expect(again.angles.size() == 2 && again.angles[0] == closeUp && again.angles[1] == distant, "angles survive");

		PF::Preset empty;
		const auto bare = PF::Read(PF::Write(empty, kWords), kWords);
		Expect(bare.error.empty() && bare.presets.size() == 1 && bare.presets.front().angles.empty(), "an empty preset round-trips");
	}

	// Situations.
	{
		namespace PR = SD::Camera::PresetRules;

		const auto file = PF::Read(
			"{ \"name\": \"Inn at night\", \"use\": \"Close\", \"priority\": 2,\n"
			"  \"when\": { \"location\": [\"Tavern\", \"Whiterun\"], \"time\": [\"night\", \"12:30-13\"], \"inside\": true,\n"
			"            \"who\": \"Merchants\", \"weather\": \"rainy\", \"conversation\": \"yours\", \"chance\": 40 },\n"
			"  \"unless\": { \"npc\": [\"Skyrim.esm|A2C94\", \"0x0A2C8E~Skyrim.esm\"] } }",
			kWords);
		Expect(file.error.empty() && file.presets.size() == 1, "a preset with conditions reads");
		const auto& preset = file.presets.front();
		Expect(preset.warnings.empty(), "valid conditions aren't reported");
		Expect(preset.use == "Close" && preset.priority == 2, "use and priority read");
		Expect(preset.when && preset.unless, "both blocks read");
		const auto& when = *preset.when;
		Expect(when.who == std::vector<std::string>{ "merchant" }, "kinds of people are stored by their first spelling");
		Expect(when.weather == std::vector<std::string>{ "rain" }, "weather too");
		Expect(when.time.size() == 2 && when.inside == true && when.overheard == false && when.chance == 40, "the rest read");
		Expect(when.Count() == 6, "chance isn't a condition");

		PR::Situation inn;
		inn.inside = true;
		inn.hour = 23.5f;
		inn.who = { "merchant", "innkeeper" };
		inn.weather = { "rain" };
		inn.location.names = { "The Bannered Mare", "Whiterun", "Whiterun Hold" };
		inn.locationKeywords = { "LocTypeInn", "LocTypeCity" };
		inn.npc.names = { "Hulda" };
		inn.npc.forms = { { "skyrim.esm", 0x13BA3, false } };
		Expect(PR::Holds(when, inn), "everything holds");
		Expect(!PR::Holds(*preset.unless, inn), "someone else isn't excepted");

		auto lydia = inn;
		lydia.npc.forms = { { "skyrim.esm", 0xA2C94, false } };
		Expect(PR::Holds(*preset.unless, lydia), "a form matches whatever load order byte was copied");

		auto noon = inn;
		noon.hour = 12.75f;
		Expect(PR::Holds(when, noon), "hour ranges hold");
		noon.hour = 13.0f;
		Expect(!PR::Holds(when, noon), "and end where they say");
		auto early = inn;
		early.hour = 3.0f;
		Expect(PR::Holds(when, early), "night runs past midnight");

		auto outside = inn;
		outside.inside = false;
		Expect(!PR::Holds(when, outside), "inside is a condition");
		auto dry = inn;
		dry.weather.clear();
		Expect(!PR::Holds(when, dry), "no weather matches indoors");
		auto city = inn;
		city.location.names = { "Riften" };
		city.locationKeywords = { "LocTypeCity" };
		Expect(!PR::Holds(when, city), "a kind of place is matched by its keywords");
		city.location.names = { "Whiterun" };
		Expect(PR::Holds(when, city), "and a location by name, anywhere in its chain");

		PR::Situation anyone;
		Expect(PR::Holds(PR::Rules{}, anyone), "an empty block always holds");

		const auto again = PF::Read(PF::Write(preset, kWords), kWords);
		Expect(again.error.empty() && again.presets.size() == 1 && again.presets.front().warnings.empty(),
			"conditions are written back readably");
		Expect(again.presets.front().when == preset.when && again.presets.front().unless == preset.unless &&
				   again.presets.front().priority == 2 && again.presets.front().use == "Close",
			"and survive the round trip");

		const auto summary = PR::Describe(when, [](std::string_view) { return std::string{}; });
		Expect(summary == "indoors, Tavern or Whiterun, night or 12:30-13, rain, merchant, your conversations, 40% of the time",
			"blocks describe themselves");
		Expect(PR::Describe(*preset.unless, [](std::string_view a_form) {
			return a_form == "Skyrim.esm|A2C94" ? std::string{ "Lydia" } : std::string{};
		}) == "Lydia or 0x0A2C8E~Skyrim.esm", "forms are named where the game knows them");

		const auto bad = PF::Read(
			"{ \"when\": { \"who\": [\"dragon\", 3], \"time\": \"teatime\", \"weather\": \"fog\", \"mood\": 1,\n"
			"              \"inside\": \"yes\", \"conversation\": \"loud\" }, \"unless\": 4, \"priority\": \"high\" }",
			kWords);
		Expect(bad.error.empty() && bad.presets.size() == 1, "bad conditions don't stop the file");
		Expect(bad.presets.front().warnings.size() == 10, "each bad condition is reported once, and what it means");
		Expect(!bad.presets.front().when, "a block of nothing but mistakes doesn't become always");
		Expect(PF::Read("{ \"when\": {} }", kWords).presets.front().when.has_value(), "but an empty block does");
		Expect(!bad.presets.front().unless, "a block that isn't one is dropped");

		Expect(!PR::ParseForm("Lydia"), "a name isn't a form");
		Expect(!PR::ParseForm("Skyrim|A2C94"), "a form names a plugin file");
		Expect(!PR::ParseForm("Skyrim.esm|xyz"), "with a hex ID");
		Expect(PR::SameForm(*PR::ParseForm("Mod.esl|FE001801"), { "mod.esl", 0x801, true }), "light plugins use 12 bits");
	}

	// Names.
	Expect(PF::Lookup(kWords.effects, "LOCKED OFF") == 0, "lookups ignore case and spaces");
	Expect(PF::Lookup(kWords.effects, "Push") == -1, "but not partial names");

	// File names.
	Expect(PF::FileStem("Tavern Talk") == "Tavern Talk", "a plain name is kept");
	Expect(PF::FileStem("a/b\\c:d*e?") == "a_b_c_d_e_", "reserved characters are replaced");
	Expect(PF::FileStem("  trailing dots... ") == "trailing dots", "trailing dots and spaces are dropped");
	Expect(PF::FileStem("") == "Preset", "an empty name gets a default");
	Expect(PF::FileStem("...") == "Preset", "a name of only dots gets a default");
	Expect(PF::FileStem("con") == "con_", "device names are avoided");
	Expect(PF::FileStem(std::string(100, 'x')).size() == 64, "long names are capped");

	// A 3-byte character straddling the cap is dropped whole.
	std::string longUtf8(63, 'x');
	longUtf8 += "\xE3\x81\x82";
	Expect(PF::FileStem(longUtf8) == std::string(63, 'x'), "a multi-byte character is never split");

	Expect(PF::OneLine(" line one\r\nline two ") == "line one  line two", "newlines become spaces");

	std::cout << "PresetFile: all tests passed\n";
	return EXIT_SUCCESS;
}
