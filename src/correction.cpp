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

#include "correction.h"

#include <algorithm>
#include <cstring>

#include "tunable.h"

namespace stormphrax {
    namespace {
        std::optional<u64> contIndex(
            const Position& pos,
            std::span<const u64> keyHistory,
            std::initializer_list<u64> offsets
        ) {
            assert(offsets.size());
            assert(std::ranges::is_sorted(offsets));

            if (keyHistory.size() < *std::prev(std::end(offsets))) {
                return std::nullopt;
            }

            u64 index = 0;

            for (const auto offset : offsets) {
                index ^= offset == 0 ? pos.key() : keyHistory[keyHistory.size() - offset];
            }

            return index;
        }
    } // namespace

    void CorrectionHistoryTable::clear() {
        std::memset(&m_tables, 0, sizeof(m_tables));
        std::memset(&m_cont, 0, sizeof(m_cont));
    }

    void CorrectionHistoryTable::update(
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval
    ) {
        auto& tables = m_tables[pos.stm().idx()];

        const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

        const auto updateCont = [&](std::initializer_list<u64> offsets) {
            if (const auto index = contIndex(pos, keyHistory, offsets)) {
                m_cont[*index % kContEntries].update(bonus);
            }
        };

        tables.pawn[pos.pawnKey() % kEntries].update(bonus);
        tables.blackNonPawn[pos.blackNonPawnKey() % kEntries].update(bonus);
        tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries].update(bonus);
        tables.major[pos.majorKey() % kEntries].update(bonus);

        updateCont({0, 1});
        updateCont({0, 2});
        updateCont({0, 4});
        updateCont({0, 1, 3, 4});
    }

    i32 CorrectionHistoryTable::correction(const Position& pos, std::span<const u64> keyHistory) const {
        using namespace tunable;

        const auto& tables = m_tables[pos.stm().idx()];

        const auto contAdjustment = [&](std::initializer_list<u64> offsets, i32 weight) {
            if (const auto index = contIndex(pos, keyHistory, offsets)) {
                return weight * m_cont[*index % kContEntries];
            } else {
                return 0;
            }
        };

        i32 correction{};

        correction += pawnCorrhistWeight() * tables.pawn[pos.pawnKey() % kEntries];
        correction += nonPawnCorrhistWeight() * tables.blackNonPawn[pos.blackNonPawnKey() % kEntries];
        correction += nonPawnCorrhistWeight() * tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries];
        correction += majorCorrhistWeight() * tables.major[pos.majorKey() % kEntries];

        correction += contAdjustment({0, 1}, contCorrhist1Weight());
        correction += contAdjustment({0, 2}, contCorrhist2Weight());
        correction += contAdjustment({0, 4}, contCorrhist4Weight());
        correction += contAdjustment({0, 1, 5, 6}, contCorrhist0156Weight());

        return correction;
    }
} // namespace stormphrax
