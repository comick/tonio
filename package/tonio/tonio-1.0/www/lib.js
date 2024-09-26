/**
 * Copyright (c) 2023-2024 Michele Comignano <mcdev@playlinux.net>
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

function libraryStageCreate() {
    const plTitle = document.getElementById('playlist-title');
    const plContainer = document.getElementById('playlist-container');
    const itemContainer = document.getElementById('item-container');
    const delButtonTpl = document.getElementById('remove-button');
    const upButtonTpl = document.getElementById('up-button');
    const downButtonTpl = document.getElementById('down-button');
    const noButtonTpl = document.getElementById('no-button');
    const resourceNameTpl = document.getElementById('item-resource');

    function playlistTitle(title, tracksTable, resources) {
        let titleElem = plTitle.content.firstChild.cloneNode(true);
        if (resources === null) {
            titleElem.innerHTML = `⚠️️ ${title}`;
        } else {
            titleElem.innerHTML = `➕ ${title}`;
            titleElem.onclick = () => {
                if (tracksTable.style.display === 'none') {
                    titleElem.innerHTML = `➖ ${title}`;
                    tracksTable.style.display = 'table';
                } else {
                    titleElem.innerHTML = `➕ ${title}`;
                    tracksTable.style.display = 'none';
                }
            };
        }
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
        let titleElem = playlistTitle(tagId, tracksElem, resources);
        if (previousTitleElem)
            rootElem.replaceChild(titleElem, previousTitleElem);
        else
            rootElem.appendChild(titleElem);

        if (previousTracksElem)
            rootElem.replaceChild(tracksElem, previousTracksElem);
        else
            rootElem.appendChild(tracksElem);

        let redraw = drawPlaylist.bind(null, tagId, rootElem, resources, false, titleElem, tracksElem);

        if (resources !== null) {
            for (let i = 0; i < resources.length; i++) {
                tracksElem.appendChild(playlistRow(i, resources, redraw));
            }
        }
    }

    return {
        open: async rootElem => {

            rootElem.innerHTML = 'Loading...';
            let libraryTags = await (await fetch('/library')).json();
            rootElem.innerHTML = '';

            libraryTags.forEach(async tagId => {
                let req = await fetch('/library/' + tagId);
                let resources = null;
                if (req.status === 200) {
                    let playlist = await req.text();
                    resources = playlist.match(/[^\n]+/g).filter(res => !res.startsWith('#'));
                }
                drawPlaylist(tagId, rootElem, resources, true);
            });
        },
        close: nop
    };
}
