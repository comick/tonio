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

function statusStageCreate() {

    const currentTagElem = document.getElementById('current-tag');
    const currentTrackElem = document.getElementById('current-track');
    const totalTracksElem = document.getElementById('total-tracks');
    const currentTrackResElem = document.getElementById('current-track-res');
    const inetConnected = document.getElementById('inet-connected');
    const inetDisconnected = document.getElementById('inet-disconnected');

    let refreshStatusTimer;

    async function refreshStatus() {
        let statusResponse = await (await fetch('/status')).json();
        currentTagElem.innerHTML = (statusResponse.present && statusResponse.id) || 'none';
        currentTrackElem.innerHTML = (typeof statusResponse.track_current === 'number') ? statusResponse.track_current : '-';
        totalTracksElem.innerHTML = (statusResponse.track_total && statusResponse.track_total) || '-';
        currentTrackResElem.innerHTML = (statusResponse.track_name && decodeURI(statusResponse.track_name)) || '-';

        if (statusResponse.internet === true) {
            inetConnected.style = 'display: block';
            inetDisconnected.style = 'display: none';
        } else {
            inetConnected.style = 'display: none';
            inetDisconnected.style = 'display: block';
        }

        refreshStatusTimer = setTimeout(refreshStatus, 2000);
    }

    return {
        open: () => {
            currentTagElem.innerHTML = 'none';
            currentTrackElem.innerHTML = '-';
            totalTracksElem.innerHTML = '-';
            currentTrackResElem.innerHTML = '-';
            inetConnected.style = 'display: none';
            inetDisconnected.style = 'display: none';

            refreshStatus();
        },
        close: () => {
            clearTimeout(refreshStatusTimer);
        }
    };
}

