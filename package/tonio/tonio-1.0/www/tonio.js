/**
 * Copyright (c) 2020, 2021 Michele Comignano <comick@gmail.com>
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
window.onload = function () {
    window.stage = (() => {
        let currentStageId;

        const nop = () => {
        };

        const stages = {

            status: (() => {
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
                    currentTrackResElem.innerHTML = (statusResponse.track_name && statusResponse.track_name) || '-';

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
            })(),

            library: (() => {
                const plTitle = document.getElementById('playlist-title');
                const plContainer = document.getElementById('playlist-container');
                const itemContainer = document.getElementById('item-container');
                const delButtonTpl = document.getElementById('remove-button');
                const upButtonTpl = document.getElementById('up-button');
                const downButtonTpl = document.getElementById('down-button');
                const noButtonTpl = document.getElementById('no-button');
                const resourceNameTpl = document.getElementById('item-resource');

                function playlistTitle(title, tracksTable) {
                    let titleElem = plTitle.content.firstChild.cloneNode(true);
                    titleElem.innerHTML = title;
                    titleElem.onclick = () => {
                        tracksTable.style.display = tracksTable.style.display === 'none' ? 'table' : 'none';
                    };
                    return titleElem;
                }

                function playlistTable(collapsed) {
                    let containerElem = plContainer.content.firstChild.cloneNode(true);
                    if (!collapsed)
                        containerElem.style.display = 'table';
                    return containerElem;
                }

                function playlistRow(pos, resources, redraw) {
                    let rowElem = itemContainer.content.firstChild.cloneNode(true);

                    rowElem.appendChild(deleteButton(pos, resources, redraw));
                    rowElem.appendChild(upButton(pos, resources, redraw));
                    rowElem.appendChild(downButton(pos, resources, redraw));

                    let resourceName = resourceNameTpl.content.firstChild.cloneNode(true);
                    resourceName.innerHTML = resources[pos];
                    rowElem.appendChild(resourceName);

                    return rowElem;
                }

                function deleteButton(pos, resources, redraw) {
                    let delButton = delButtonTpl.content.firstChild.cloneNode(true);
                    delButton.onclick = () => {
                        resources.splice(pos, 1);
                        redraw();
                    };
                    return delButton;
                }

                function upButton(pos, resources, redraw) {
                    if (pos === 0)
                        return noButtonTpl.content.firstChild.cloneNode(true);
                    else {
                        let upButton = upButtonTpl.content.firstChild.cloneNode(true);
                        upButton.onclick = () => {
                            let tmp = resources[pos];
                            resources[pos] = resources[pos - 1];
                            resources[pos - 1 ] = tmp;
                            redraw();
                        };
                        return upButton;
                    }
                }

                function downButton(pos, resources, redraw) {
                    if (pos === resources.length - 1)
                        return noButtonTpl.content.firstChild.cloneNode(true);
                    else {
                        let downButton = downButtonTpl.content.firstChild.cloneNode(true);
                        downButton.onclick = () => {
                            let tmp = resources[pos];
                            resources[pos] = resources[pos + 1];
                            resources[pos + 1] = tmp;
                            redraw();
                        };
                        return downButton;
                    }
                }

                function drawPlaylist(tagId, rootElem, resources, collapsed, previousTitleElem, previousTracksElem) {
                    let tracksElem = playlistTable(collapsed);
                    let titleElem = playlistTitle(tagId, tracksElem);
                    if (previousTitleElem)
                        rootElem.replaceChild(titleElem, previousTitleElem);
                    else
                        rootElem.appendChild(titleElem);

                    if (previousTracksElem)
                        rootElem.replaceChild(tracksElem, previousTracksElem);
                    else
                        rootElem.appendChild(tracksElem);

                    let redraw = drawPlaylist.bind(null, tagId, rootElem, resources, false, titleElem, tracksElem);

                    for (let i = 0; i < resources.length; i++) {
                        tracksElem.appendChild(playlistRow(i, resources, redraw));
                    }
                }

                return {
                    open: async rootElem => {

                        rootElem.innerHTML = 'Loading...';
                        let libraryTags = await (await fetch('/library')).json();
                        rootElem.innerHTML = '';

                        libraryTags.forEach(async tagId => {
                            let playlist = await (await fetch('/library/' + tagId)).text();
                            let resources = playlist.match(/[^\n]+/g).filter(res => !res.startsWith('#'));
                            drawPlaylist(tagId, rootElem, resources, true);
                        });
                    },
                    close: nop
                };
            })(),

            log: (() => {
                let refreshLogTimeout;

                async function refreshLog(rootElem, args) {
                    let buf = await (await fetch('/log?offset=' + args.offset)).arrayBuffer();
                    let log = new TextDecoder("utf-8").decode(buf);
                    args.offset += buf.byteLength;
                    log.match(/[^\n]+/g).forEach(trace => {
                        let line = document.createElement('div');
                        line.innerHTML = trace;
                        if (trace.includes("user.info tonio")) {
                            line.className = "log info";
                        } else if (trace.includes("user.err tonio")) {
                            line.className = "log error";
                        } else if (trace.includes("user.warn tonio")) {
                            line.className = "log warn";
                        } else if (trace.includes("user.debug tonio")) {
                            line.className = "log debug";
                        } else {
                            line.className = "log other";
                        }
                        rootElem.prepend(line);
                    });
                    clearTimeout(refreshLogTimeout);
                    refreshLogTimeout = setTimeout(refreshLog.bind(null, rootElem, args), 2000);
                }

                return {
                    open: rootElem => {
                        rootElem.innerHTML = "";
                        let args = {offset: 0};
                        refreshLog(rootElem, args);
                    },
                    close: () => clearTimeout(refreshLogTimeout)
                };
            })(),

            settings: (() => {

                return {
                    open: async () => {
                        const iwlistReload = document.getElementById('iwlist-reload');
                        const iwlist = document.getElementById('iwlist');
                        const gpioPrev = document.getElementById('btn-track-previous');
                        const gpioNext = document.getElementById('btn-track-next');
                        const gpioVolup = document.getElementById('btn-volume-up');
                        const gpioVoldown = document.getElementById('btn-volume-down');
                        const spiDev = document.getElementById('mfrc522-spi-dev');
                        const gpioChipName = document.getElementById('gpiod-chip-name');
                        const settingsForm = document.getElementById('settings-form');

                        iwlist.innerHTML = '';

                        async function loadIwlist(currentEssid) {
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
                            iwlistReload.onclick = loadIwlist.bind(null, currentEssid);
                        }

                        let settings = await (await fetch('/settings')).json();
                        gpioPrev.value = settings.pin_prev;
                        gpioNext.value = settings.pin_next;
                        gpioVolup.value = settings.pin_volup;
                        gpioVoldown.value = settings.pin_voldown;
                        spiDev.value = settings.spi_rfid;
                        gpioChipName.value = settings.gpio_chip;
                        loadIwlist(settings.essid);

                        settingsForm.onsubmit = (event) => {
                            let url = "/settings";
                            // TODO fetch
                            let request = new XMLHttpRequest();
                            request.open('POST', url, true);
                            request.onload = function () { // request successful
                                // we can use server response to our request now
                                console.log(request.responseText);
                            };
                            request.onerror = function () {
                                // request failed
                            };
                            request.send(new FormData(event.target)); // create FormData from form that triggered event
                            return false;
                        };


                    },
                    close: () => {
                    }
                };
            })(),

            about: (() => ({open: nop, close: nop}))()

        };

        return (stageId) => {

            if (!stages.hasOwnProperty(stageId))
                return false;

            if (currentStageId) {
                let previousRootElem = document.getElementById(currentStageId + '-stage');
                previousRootElem.style = 'display: none';
                stages[currentStageId].close(previousRootElem);
                document.getElementById(currentStageId + '-href').classList.toggle('active');
            }

            currentStageId = stageId;
            let currentRootElem = document.getElementById(currentStageId + '-stage');
            stages[currentStageId].open(currentRootElem);
            currentRootElem.style = 'display: block';
            window.location.hash = '#' + currentStageId;
            document.getElementById(currentStageId + '-href').classList.toggle('active');
            return true;
        };

    })();

    let hash = window.location.hash.substr(1);
    (hash && stage(hash)) || stage('status');
};
