#pragma once

namespace SD::Camera
{
	// Presets that say when to use them take over one conversation at a time.
	//
	// An installed preset with a "when" block (see PresetRules.h) is a candidate at
	// the start of every conversation. The one chosen is put onto the live settings
	// only: nothing is written to SD_user.ini, and your own settings come back when
	// the conversation closes. A setting changed in the menu while it's in force is
	// kept rather than undone.
	class Situational
	{
	public:
		// After the settings are read for a new conversation. a_partner is who the
		// camera opened on; a_overheard means filming other people.
		static void Begin(RE::Actor* a_partner, bool a_overheard);

		// The same conversation back after a menu or a release: whatever was chosen
		// for it goes back on.
		static void Resume();

		// The conversation closed or was suspended.
		static void End();

		// The name of the preset in force now, or empty.
		[[nodiscard]] static std::string Current();

		// Who the latest conversation was with, so the menu can offer them as a
		// condition. form is empty for someone the game spawned.
		struct Person
		{
			std::string name;
			std::string form;
		};
		[[nodiscard]] static std::optional<Person> LastPerson();

		// The name of a form written as "Plugin.esp|ID", or empty.
		[[nodiscard]] static std::string NameOf(std::string_view a_form);
	};
}
