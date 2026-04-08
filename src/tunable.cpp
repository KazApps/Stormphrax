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

#include "tunable.h"

#include <cmath>

namespace stormphrax::tunable {
    std::array<i32, 13> g_seeValues{};

    void updateSeeValueTable() {
        // king and none
        g_seeValues.fill(0);

        const std::array scores = {
            seeValuePawn(),
            seeValueKnight(),
            seeValueBishop(),
            seeValueRook(),
            seeValueQueen(),
        };

        for (usize i = 0; i < scores.size(); ++i) {
            g_seeValues[i * 2 + 0] = scores[i];
            g_seeValues[i * 2 + 1] = scores[i];
        }
    }

    void init() {;
        updateSeeValueTable();
    }
} // namespace stormphrax::tunable
