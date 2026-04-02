/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
 *
 * Stormphrax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stormphrax is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stormphrax. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include <algorithm>
#include <atomic>
#include <cstring>

#include "core.h"
#include "position/position.h"
#include "tunable.h"
#include "util/cemath.h"
#include "util/multi_array.h"

namespace stormphrax {
    struct PlayedMove {
        Piece moving;
        Square dst;
    };

    class CorrectionHistoryTable {
    public:
        inline void clear() {
            std::memset(&m_tables, 0, sizeof(m_tables));
        }

        inline void update(
            const Position& pos,
            std::span<const u64> keyHistory,
            i32 depth,
            Score searchScore,
            Score staticEval
        ) {
            auto& tables = m_tables[pos.stm().idx()];

            const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

            const auto updateCont = [&](const u64 offset) {
                if (keyHistory.size() >= offset) {
                    tables.cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries].update(bonus);
                }
            };

            tables.pawn[pos.pawnKey() % kEntries].update(bonus);
            tables.blackNonPawn[pos.blackNonPawnKey() % kEntries].update(bonus);
            tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries].update(bonus);
            tables.major[pos.majorKey() % kEntries].update(bonus);

            updateCont(1);
            updateCont(2);
            updateCont(4);
        }

        [[nodiscard]] inline Score correct(const Position& pos, std::span<const u64> keyHistory, Score score) const {
            using namespace tunable;

            auto& tables = m_tables[pos.stm().idx()];

            const auto contAdjustment = [&](const u64 offset, i32 weight) {
                if (keyHistory.size() >= offset) {
                    return weight * tables.cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries];
                } else {
                    return 0;
                }
            };

            const auto [blackNpWeight, whiteNpWeight] =
                pos.stm() == Colors::kBlack ? std::pair{stmNonPawnCorrhistWeight(), nstmNonPawnCorrhistWeight()}
                                            : std::pair{nstmNonPawnCorrhistWeight(), stmNonPawnCorrhistWeight()};

            const i32 c1 = pawnCorrhistWeight() * tables.pawn[pos.pawnKey() % kEntries];
            const i32 c2 = blackNpWeight * tables.blackNonPawn[pos.blackNonPawnKey() % kEntries];
            const i32 c3 = whiteNpWeight * tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries];
            const i32 c4 = majorCorrhistWeight() * tables.major[pos.majorKey() % kEntries];
            const i32 c5 = contAdjustment(1, contCorrhist1Weight());
            const i32 c6 = contAdjustment(2, contCorrhist2Weight());
            const i32 c7 = contAdjustment(4, contCorrhist4Weight());

            const i32 avg = (c1 + c2 + c3 + c4 + c5 + c6 + c7) / 7;

            const auto cv = [&](i32 c) { return std::abs(c - avg) < 16384 ? c : 0; };
            const auto correction = cv(c1) + cv(c2) + cv(c3) + cv(c4) + cv(c5) + cv(c6) + cv(c7);

            score += correction / 2048;

            return std::clamp(score, -kScoreWin + 1, kScoreWin - 1);
        }

    private:
        static constexpr usize kEntries = 16384;

        static constexpr i32 kLimit = 1024;
        static constexpr i32 kMaxBonus = kLimit / 4;

        struct Entry {
            std::atomic<i16> value{};

            inline void update(i32 bonus) {
                auto v = value.load(std::memory_order::relaxed);
                v += bonus - v * std::abs(bonus) / kLimit;
                value.store(v, std::memory_order::relaxed);
            }

            [[nodiscard]] inline operator i32() const {
                return value.load(std::memory_order::relaxed);
            }
        };

        struct SidedTables {
            std::array<Entry, kEntries> pawn{};
            std::array<Entry, kEntries> blackNonPawn{};
            std::array<Entry, kEntries> whiteNonPawn{};
            std::array<Entry, kEntries> major{};
            std::array<Entry, kEntries> cont{};
        };

        std::array<SidedTables, Colors::kCount> m_tables{};
    };
} // namespace stormphrax
