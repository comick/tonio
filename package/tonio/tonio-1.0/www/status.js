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
        let resp = await (await fetch('/status')).json();
        currentTagElem.innerHTML = (resp.card_id && `#${resp.card_id.toString(16).toUpperCase()}`) || '-';
        currentTrackElem.innerHTML = (typeof resp.track_current === 'number') ? resp.track_current : '-';
        totalTracksElem.innerHTML = (resp.track_total && resp.track_total) || '-';
        currentTrackResElem.innerHTML = (resp.track_name && decodeURI(resp.track_name)) || '-';

        if (resp.internet === true) {
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

