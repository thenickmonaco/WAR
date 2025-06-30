// vimDAW - make music with vim motions
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

//=============================================================================
// build.rs
//=============================================================================

//=============================================================================
// for debugging/reference purposes
//=============================================================================

use std::fs::File;
use std::io::BufReader;
use xmltree::Element;

fn main() {
    let path = std::env::var("WAYLAND_XML_PATH")
        .unwrap_or_else(|_| "/usr/share/wayland/wayland.xml".to_string());
    let file = File::open(path).expect("Cannot open XML");
    let root = Element::parse(BufReader::new(file)).unwrap();

    for iface in root
        .children
        .iter()
        .filter_map(|n| n.as_element())
        .filter(|e| e.name == "interface")
    {
        let iface_name = iface.attributes.get("name").unwrap();

        println!("Interface: {iface_name}");
        for req in iface
            .children
            .iter()
            .filter_map(|n| n.as_element())
            .filter(|e| e.name == "request" || e.name == "event")
        {
            let req_name = req.attributes.get("name").unwrap();
            let opcode = req
                .attributes
                .get("opcode")
                .map(|s| s.as_str())
                .unwrap_or("?");
            println!("  {:?} opcode={} name={}", req.name, opcode, req_name);
        }
    }
}
