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

function settingsStageCreate() {
    const settingsInput = {
        'pin_prev': document.getElementById('btn-track-previous'),
        'pin_next': document.getElementById('btn-track-next'),
        'pin_volup': document.getElementById('btn-volume-up'),
        'pin_voldown': document.getElementById('btn-volume-down'),
        'spi_rfid': document.getElementById('mfrc522-spi-dev'),
        'gpio_chip': document.getElementById('gpiod-chip-name')
    };
    const iwlistReload = document.getElementById('iwlist-reload');
    const iwlist = document.getElementById('iwlist');

    const applyButton = document.getElementById('settings-apply');

    async function loadSettings() {
        let settings = await(await fetch('/settings')).json();

        iwlist.innerHTML = '';
        await loadIwlist(settings.essid, iwlist, iwlistReload);

        Object.keys(settingsInput).forEach(k => {
            settingsInput[k].value = settings[k];
            settingsInput[k].disabled = false;
        });
        applyButton.disabled = false;
    }

    return {
        open: async () => {
            await loadSettings();

            applyButton.onclick = async () => {

                let body = new FormData();
                Object.keys(settingsInput).forEach(k => {
                    body.append(k, settingsInput[k].value);
                    settingsInput[k].disabled = true;
                });
                applyButton.disabled = true;

                let resp = await fetch('/settings', {
                    method: 'POST',
                    body: body
                });
                if (resp.status === 200) {
                    // Service reloads after post settings, wait until responsive.
                    while (true) {
                        try {
                            await loadSettings();
                            break;
                        } catch {
                            continue;
                        }
                    }
                }
            };
        },
        close: nop
    };
}
