#include "SD/Dialogue/SubtitleTap.h"

#include "SD/Core/Logging.h"
#include "SD/Dialogue/SubtitleCode.h"

#include <deque>

namespace SD::Dialogue
{
	namespace
	{
		// SubtitleManager's add: the manager, the speaking reference, the text, and
		// whether to show it at any distance. Every argument register goes through
		// untouched and rax comes back, so the hook adds nothing to the call itself.
		using AddFn = std::uintptr_t (*)(RE::SubtitleManager*, RE::TESObjectREFR*, const char*, std::uintptr_t);

		// 1.5.97. Other runtimes are found by the settings test instead.
		constexpr std::uint64_t kAddSubtitleSE = 51753;

		// The settings test sits about 0x80 bytes into the add on 1.5.97.
		constexpr std::size_t kTestWithin = 0x180;

		constexpr std::size_t kMaxCaught = 16;
		constexpr std::size_t kMaxText = 512;

		AddFn             original = nullptr;
		RE::Setting*      general = nullptr;
		RE::Setting*      dialogue = nullptr;
		std::atomic<bool> installed{ false };

		std::mutex                         lock;
		std::deque<SubtitleTap::Caught>    caught;

		[[nodiscard]] bool BothOff() noexcept
		{
			return general && dialogue && !general->data.b && !dialogue->data.b;
		}

		std::uintptr_t Add(RE::SubtitleManager* a_this, RE::TESObjectREFR* a_speaker, const char* a_text,
			std::uintptr_t a_force)
		{
			const auto result = original(a_this, a_speaker, a_text, a_force);

			// Only what the engine just turned away. With either setting on the line is in
			// its list, and SceneWatch reads it there.
			if (a_speaker && a_text && *a_text && BothOff() && !a_speaker->IsPlayerRef() &&
				a_speaker->GetFormType() == RE::FormType::ActorCharacter) {
				try {
					SubtitleTap::Caught line{ a_speaker->GetFormID(), std::string{ a_text, ::strnlen(a_text, kMaxText) },
						std::chrono::steady_clock::now() };
					const std::scoped_lock guard{ lock };
					if (caught.size() >= kMaxCaught) {
						caught.pop_front();
					}
					caught.push_back(std::move(line));
				} catch (...) {
				}
			}
			return result;
		}

		[[nodiscard]] std::span<const std::uint8_t> Bytes(std::uintptr_t a_address, std::size_t a_size)
		{
			return { reinterpret_cast<const std::uint8_t*>(a_address), a_size };
		}

		[[nodiscard]] RE::Setting* FindSetting(std::string_view a_name)
		{
			if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton()) {
				if (auto* setting = prefs->GetSetting(a_name)) {
					return setting;
				}
			}
			auto* ini = RE::INISettingCollection::GetSingleton();
			return ini ? ini->GetSetting(a_name) : nullptr;
		}

		[[nodiscard]] bool IsAdd(std::uintptr_t a_function, std::uintptr_t a_general, std::uintptr_t a_dialogue)
		{
			return a_function &&
			       SubtitleCode::FindEitherSettingTest(Bytes(a_function, kTestWithin), a_function, a_general,
					   a_dialogue) != SubtitleCode::kNotFound;
		}

		// The start of the function holding a_pc, from the unwind table. A function the
		// compiler split into pieces keeps its real start at the end of the chain.
		[[nodiscard]] std::uintptr_t FunctionStart(std::uintptr_t a_pc)
		{
			DWORD64 imageBase = 0;
			auto*   entry = ::RtlLookupFunctionEntry(a_pc, &imageBase, nullptr);
			for (int depth = 0; entry && depth < 8; ++depth) {
				if (entry->UnwindData & 1) {
					entry = reinterpret_cast<PRUNTIME_FUNCTION>(imageBase + (entry->UnwindData & ~1u));
					continue;
				}
				const auto* info = reinterpret_cast<const std::uint8_t*>(imageBase + entry->UnwindData);
				if (((info[0] >> 3) & UNW_FLAG_CHAININFO) == 0) {
					return static_cast<std::uintptr_t>(imageBase + entry->BeginAddress);
				}
				const std::size_t codes = (static_cast<std::size_t>(info[2]) + 1) & ~std::size_t{ 1 };
				entry = reinterpret_cast<PRUNTIME_FUNCTION>(const_cast<std::uint8_t*>(info) + 4 + codes * 2);
			}
			return 0;
		}

		// Every place in the game's code that tests the two settings back to back; the
		// add is the one whose function starts within reach of it.
		[[nodiscard]] std::uintptr_t Search(std::uintptr_t a_general, std::uintptr_t a_dialogue)
		{
			const auto text = REL::Module::get().segment(REL::Segment::textx);
			const auto code = Bytes(text.address(), text.size());

			std::size_t from = 0;
			while (from < code.size()) {
				const auto at = SubtitleCode::FindEitherSettingTest(code.subspan(from), text.address() + from,
					a_general, a_dialogue);
				if (at == SubtitleCode::kNotFound) {
					break;
				}
				const auto site = text.address() + from + at;
				const auto start = FunctionStart(site);
				if (start && site - start < kTestWithin && IsAdd(start, a_general, a_dialogue)) {
					return start;
				}
				from += at + 1;
			}
			return 0;
		}
	}

	void SubtitleTap::Install()
	{
		if (installed.load(std::memory_order_relaxed)) {
			return;
		}

		general = FindSetting("bGeneralSubtitles:Interface"sv);
		dialogue = FindSetting("bDialogueSubtitles:Interface"sv);
		if (!general || !dialogue) {
			Log::Warn(Log::Category::kDialogue,
				"The game's subtitle settings weren't found. With General and Dialogue Subtitles both off, "
				"other people's conversations can't be heard or filmed."sv);
			return;
		}
		const auto generalAt = reinterpret_cast<std::uintptr_t>(&general->data.b);
		const auto dialogueAt = reinterpret_cast<std::uintptr_t>(&dialogue->data.b);

		std::uintptr_t   add = 0;
		std::string_view how{};
		if (REL::Module::IsSE()) {
			const auto byId = REL::ID(kAddSubtitleSE).address();
			if (IsAdd(byId, generalAt, dialogueAt)) {
				add = byId;
				how = "by its Address Library ID"sv;
			}
		}
		if (!add) {
			add = Search(generalAt, dialogueAt);
			how = "by the settings test inside it"sv;
		}
		if (!add) {
			Log::Warn(Log::Category::kDialogue,
				"Couldn't find where this game version adds subtitles. With General and Dialogue Subtitles both "
				"off, other people's conversations can't be heard or filmed."sv);
			return;
		}

		const auto entry = SubtitleCode::ReadEntry(Bytes(add, 32), add);
		std::size_t    copied = 0;
		std::uintptr_t resume = 0;
		switch (entry.kind) {
		case SubtitleCode::Entry::Kind::kCode:
			copied = entry.length;
			resume = add + entry.length;
			break;
		case SubtitleCode::Entry::Kind::kJump:
			resume = entry.target;
			break;
		case SubtitleCode::Entry::Kind::kIndirect:
			resume = *reinterpret_cast<const std::uintptr_t*>(entry.pointer);
			break;
		case SubtitleCode::Entry::Kind::kUnknown:
			break;
		}
		if (!resume) {
			std::string head;
			for (const auto byte : Bytes(add, 8)) {
				head += fmt::format("{:02X} ", byte);
			}
			Log::Warn(Log::Category::kDialogue,
				"The game's subtitle add at {:X} starts with code that can't be moved ({}). With General and "
				"Dialogue Subtitles both off, other people's conversations can't be heard or filmed."sv,
				add, head);
			return;
		}

		// The moved bytes, then an absolute jump to wherever the function carries on.
		constexpr std::array<std::uint8_t, 6> kJump{ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
		SKSE::AllocTrampoline(64);
		auto& trampoline = SKSE::GetTrampoline();
		auto* gate = static_cast<std::uint8_t*>(trampoline.allocate(copied + kJump.size() + sizeof(resume)));
		std::memcpy(gate, reinterpret_cast<const void*>(add), copied);
		std::memcpy(gate + copied, kJump.data(), kJump.size());
		std::memcpy(gate + copied + kJump.size(), &resume, sizeof(resume));
		original = reinterpret_cast<AddFn>(gate);

		// Anything past the first five bytes is never reached once the jump is in.
		trampoline.write_branch<5>(add, &Add, entry.length != 5);
		if (entry.kind == SubtitleCode::Entry::Kind::kCode && entry.length > 5) {
			REL::safe_fill(add + 5, 0xCC, entry.length - 5);
		}

		installed.store(true, std::memory_order_relaxed);
		Log::Info(Log::Category::kDialogue,
			"Subtitle tap on the game's subtitle add at {:X} (found {}{}). Other people's lines are heard with "
			"subtitles off too. Subtitles are {} right now."sv,
			add, how, entry.kind == SubtitleCode::Entry::Kind::kCode ? ""sv : ", after another mod's hook"sv,
			BothOff() ? "off"sv : "on"sv);
	}

	bool SubtitleTap::Installed() noexcept
	{
		return installed.load(std::memory_order_relaxed);
	}

	bool SubtitleTap::SubtitlesOff() noexcept
	{
		return BothOff();
	}

	std::vector<SubtitleTap::Caught> SubtitleTap::Take()
	{
		std::vector<Caught> lines;
		const std::scoped_lock guard{ lock };
		lines.reserve(caught.size());
		for (auto& line : caught) {
			lines.push_back(std::move(line));
		}
		caught.clear();
		return lines;
	}
}
