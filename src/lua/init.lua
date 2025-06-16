-- vimDAW - make music with vim motions
-- Copyright (C) 2025 Nick Monaco
-- 
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Affero General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU Affero General Public License for more details.
-- 
-- You should have received a copy of the GNU Affero General Public License
-- along with this program. If not, see <https://www.gnu.org/licenses/>.

--=============================================================================
-- src/lua/init.lua
--=============================================================================

vimdaw = {
    keymap = {
        normal = {
            ["k"] = function() vimdaw.up(1) end,
            ["j"] = function() vimdaw.down(1) end,
            ["h"] = function() vimdaw.left(1) end,
            ["l"] = function() vimdaw.right(1) end,
            ["C-h"] = function() vimdaw.hello_world() end
        },
        visual = {

        },
    },

    theme = {
        default = {
            fg = "#ffffffff",
            bg = "#00000000",
        }
    },

    opt = {
        default = {
            cursor_col = 0,
            cursor_row = 60,
        }
    }
}
