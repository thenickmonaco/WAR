//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

asdfljk
ajsdflkasjdflsakdjf
jalkfsdjjafs
asdflkjsalkfjdsakldjfalskdjfsalkfdj
alsdfjaslkdfj
asdjflkasjdflksjdflaskjdfksajfd
asldfjaskdfj
asdfjlkasjdfl
asjdfklasjd 
asdlkfsja
alsdkjfasklf
#include "war_drm.c"
#include "war_vulkan.c"
#include "war_wayland.c"

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_main.h"

#include <stdint.h>
#include <unistd.h>

int main() {
    CALL_CARMACK("main");

    // while (1) {
    // wayland_handle_message();
    // if (render) {
    // render();
    // }
    // if (input) {
    // input();
    // }
    // }

    war_wayland_init();

    END("main");
    return 0;
}
