#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// Reading the engine's subtitle code, for SubtitleTap. Pure byte work, so it can
// be tested against the real 1.5.97 bytes without a game.
namespace SD::Dialogue::SubtitleCode
{
	inline constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

	[[nodiscard]] inline std::int32_t ReadDisp(std::span<const std::uint8_t> a_code, std::size_t a_at) noexcept
	{
		std::int32_t value = 0;
		std::memcpy(&value, a_code.data() + a_at, sizeof(value));
		return value;
	}

	// a_from plus a signed 32-bit displacement, wrapping like the CPU does.
	[[nodiscard]] constexpr std::uintptr_t Displace(std::uintptr_t a_from, std::int32_t a_disp) noexcept
	{
		return a_from + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(a_disp));
	}

	// Where code tests two bool settings back to back, each as
	// `cmp byte ptr [rip+disp], 0`, with at most a short branch between them. That
	// is how the engine asks "is either of these on" before it keeps a subtitle.
	// a_address is where a_code starts in memory; a_first and a_second are the
	// addresses of the two settings' values. Returns the offset of the first test.
	[[nodiscard]] inline std::size_t FindEitherSettingTest(std::span<const std::uint8_t> a_code,
		std::uintptr_t a_address, std::uintptr_t a_first, std::uintptr_t a_second) noexcept
	{
		constexpr std::size_t kTest = 7;    // 80 3D disp32 00
		constexpr std::size_t kBetween = 16;

		if (a_code.size() < kTest * 2) {
			return kNotFound;
		}
		const auto isTest = [&](std::size_t a_at) {
			return a_code[a_at] == 0x80 && a_code[a_at + 1] == 0x3D && a_code[a_at + 6] == 0x00;
		};
		const auto target = [&](std::size_t a_at) {
			return Displace(a_address + a_at + kTest, ReadDisp(a_code, a_at + 2));
		};

		const std::size_t lastStart = a_code.size() - kTest;
		for (std::size_t i = 0; i + kTest <= lastStart; ++i) {
			if (!isTest(i) || target(i) != a_first) {
				continue;
			}
			const std::size_t until = std::min(i + kTest + kBetween, lastStart);
			for (std::size_t j = i + kTest; j <= until; ++j) {
				if (isTest(j) && target(j) == a_second) {
					return i;
				}
			}
		}
		return kNotFound;
	}

	// Length of one prologue instruction that runs the same from any address, or 0
	// for anything that doesn't: RIP-relative operands, branches, and anything not
	// recognised. Only the handful of forms compilers open functions with.
	[[nodiscard]] inline std::size_t MovableLength(std::span<const std::uint8_t> a_code, std::size_t a_at) noexcept
	{
		std::size_t i = a_at;
		const auto have = [&](std::size_t a_count) { return i + a_count <= a_code.size(); };

		if (!have(1)) {
			return 0;
		}
		if (a_code[i] >= 0x40 && a_code[i] <= 0x4F) {  // REX
			++i;
			if (!have(1)) {
				return 0;
			}
		}

		const std::uint8_t op = a_code[i++];
		if (op >= 0x50 && op <= 0x57) {  // push
			return i - a_at;
		}

		std::size_t immediate = 0;
		switch (op) {
		case 0x31:  // xor r/m, r
		case 0x33:  // xor r, r/m
		case 0x85:  // test
		case 0x89:  // mov r/m, r
		case 0x8B:  // mov r, r/m
		case 0x8D:  // lea
			break;
		case 0x83:  // arithmetic with imm8 (sub rsp, n)
			immediate = 1;
			break;
		case 0x81:  // arithmetic with imm32
		case 0xC7:  // mov r/m, imm32
			immediate = 4;
			break;
		default:
			return 0;
		}

		if (!have(1)) {
			return 0;
		}
		const std::uint8_t modrm = a_code[i++];
		const unsigned mod = modrm >> 6;
		const unsigned rm = modrm & 7u;

		if (mod == 0 && rm == 5) {
			return 0;  // RIP-relative: would point somewhere else once moved
		}
		if (mod != 3 && rm == 4) {
			if (!have(1)) {
				return 0;
			}
			const std::uint8_t sib = a_code[i++];
			if (mod == 0 && (sib & 7u) == 5) {
				i += 4;
			}
		}
		if (mod == 1) {
			i += 1;
		} else if (mod == 2) {
			i += 4;
		}
		i += immediate;
		return i <= a_code.size() ? i - a_at : 0;
	}

	// What a function starts with, for moving it aside to make room for a 5-byte
	// jump.
	struct Entry
	{
		enum class Kind : std::uint8_t
		{
			kUnknown,   // can't be moved safely; leave the function alone
			kCode,      // ordinary instructions: copy `length` bytes, then carry on after them
			kJump,      // someone else's `jmp rel32`: go where it went
			kIndirect,  // someone else's `jmp [rip+disp]`: go where the pointer at `pointer` says
		};

		Kind           kind{ Kind::kUnknown };
		std::size_t    length{ 0 };
		std::uintptr_t target{ 0 };
		std::uintptr_t pointer{ 0 };
	};

	[[nodiscard]] inline Entry ReadEntry(std::span<const std::uint8_t> a_code, std::uintptr_t a_address,
		std::size_t a_room = 5) noexcept
	{
		Entry entry{};
		if (a_code.size() >= 5 && a_code[0] == 0xE9) {
			entry.kind = Entry::Kind::kJump;
			entry.length = 5;
			entry.target = Displace(a_address + 5, ReadDisp(a_code, 1));
			return entry;
		}
		if (a_code.size() >= 6 && a_code[0] == 0xFF && a_code[1] == 0x25) {
			entry.kind = Entry::Kind::kIndirect;
			entry.length = 6;
			entry.pointer = Displace(a_address + 6, ReadDisp(a_code, 2));
			return entry;
		}

		std::size_t length = 0;
		while (length < a_room) {
			const std::size_t step = MovableLength(a_code, length);
			if (step == 0) {
				return Entry{};
			}
			length += step;
		}
		entry.kind = Entry::Kind::kCode;
		entry.length = length;
		return entry;
	}
}
