/**
 * Copyright (c) 2023 Michele Comignano <mcdev@playlinux.net>
 * This file is part of Tonio.
 *
 * Tonio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tonio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tonio.  If not, see <http://www.gnu.org/licenses/>.
 */

const nop = () => {
};

async function loadIwlist(currentEssid, iwlist, iwlistReload) {
    iwlist.disabled = true;
    iwlistReload.className = 'disabled';
    iwlistReload.onclick = null;
    let networks = await (await fetch('/iwlist')).json();
    iwlist.innerHTML = '';
    networks.forEach(essid => {
        let network = document.createElement('option');
        if (essid === currentEssid) {
            network.innerHTML = `${essid} (current)`;
            network.selected = 'selected';
        } else {
            network.innerHTML = essid;
        }
        network.value = essid;
        iwlist.appendChild(network);
    });
    iwlist.disabled = false;
    iwlistReload.className = 'enabled';
    iwlistReload.onclick = loadIwlist.bind(null, currentEssid, iwlist, iwlistReload);
}
