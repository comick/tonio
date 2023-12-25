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

function setupStageCreate() {
    let settings;
    let finalizeButton;

    async function finalize() {
        settings['factory-new'] = false;

        let body = new FormData();
        Object.keys(settings).forEach(k => body.append(k, settings[k]));

        let resp = await fetch('/settings', {
            method: 'POST',
            body: body
        });
        if (resp.status === 200) {
            // Service reloads after post settings, wait until responsive.
            while (true) {
                try {
                    await fetch('/status');
                    return stage('status');
                } catch {
                }
            }
        }
    }

    return {
        open: async () => {
            settings = await (await fetch('/settings')).json();
            if (settings['factory-new'] === false) {
                stage('status');
            }
            finalizeButton = document.getElementById('setup-finalize');
            finalizeButton.onclick = finalize;
        },
        close: nop
    };
}
