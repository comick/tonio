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
    const saveButtonTpl = document.getElementById('save-button');
    const dragHandleTpl = document.getElementById('drag-handle');
    const resourceNameTpl = document.getElementById('item-resource');

    const originalResources = {};

    function playlistTitle(tagId, title, tracksList, resources, redraw) {
        let titleElem = plTitle.content.firstChild.cloneNode(true);
        let titleText = document.createElement('span');
        titleElem.innerHTML = '';
        titleElem.appendChild(titleText);

        if (resources === null) {
            titleText.innerHTML = `⚠️️ ${title}`;
        } else {
            titleText.innerHTML = `▸ ${title}`;
            titleText.onclick = () => {
                if (tracksList.style.display === 'none') {
                    titleText.innerHTML = `▾ ${title}`;
                    tracksList.style.display = 'block';
                } else {
                    titleText.innerHTML = `▸ ${title}`;
                    tracksList.style.display = 'none';
                }
            };

            const original = originalResources[tagId];
            const changed = original && (original.length !== resources.length || resources.some((v, i) => v !== original[i]));

            if (changed) {
                let saveButton = saveButtonTpl.content.firstChild.cloneNode(true);
                saveButton.onclick = async () => {
                    let body = resources.join('\n') + '\n';
                    let req = await fetch('/library/' + tagId, {
                        method: 'POST',
                        body: body
                    });
                    if (req.status === 200) {
                        originalResources[tagId] = [...resources];
                        saveButton.remove();
                    }
                };
                saveButton.style.float = 'right';
                titleElem.appendChild(saveButton);
            }
        }
        return titleElem;
    }

    function playlistList(collapsed) {
        let containerElem = plContainer.content.firstChild.cloneNode(true);
        if (!collapsed)
            containerElem.style.display = 'block';
        return containerElem;
    }

    function playlistItem(pos, resources, redraw) {
        let itemElem = itemContainer.content.firstChild.cloneNode(true);

        itemElem.draggable = true;
        itemElem.ondragstart = e => {
            e.dataTransfer.setData('text/plain', pos);
        };
        itemElem.ondragover = e => {
            e.preventDefault();
            const rect = itemElem.getBoundingClientRect();
            const relY = e.clientY - rect.top;
            if (relY < rect.height / 2) {
                itemElem.classList.add('drag-over-top');
                itemElem.classList.remove('drag-over-bottom');
            } else {
                itemElem.classList.add('drag-over-bottom');
                itemElem.classList.remove('drag-over-top');
            }
        };
        itemElem.ondragenter = e => {
            e.preventDefault();
        };
        itemElem.ondragleave = e => {
            itemElem.classList.remove('drag-over-top');
            itemElem.classList.remove('drag-over-bottom');
        };
        itemElem.ondrop = e => {
            e.preventDefault();
            const rect = itemElem.getBoundingClientRect();
            const relY = e.clientY - rect.top;
            const dropAtBottom = relY >= rect.height / 2;

            itemElem.classList.remove('drag-over-top');
            itemElem.classList.remove('drag-over-bottom');

            let from = parseInt(e.dataTransfer.getData('text/plain'));
            let to = pos;

            if (dropAtBottom) to++;

            if (from !== to) {
                let item = resources.splice(from, 1)[0];
                if (from < to) to--;
                resources.splice(to, 0, item);
                redraw();
            }
        };

        itemElem.appendChild(deleteButton(pos, resources, redraw));
        itemElem.appendChild(dragHandleTpl.content.firstChild.cloneNode(true));

        let resourceName = resourceNameTpl.content.firstChild.cloneNode(true);
        resourceName.innerHTML = resources[pos];
        itemElem.appendChild(resourceName);

        return itemElem;
    }

    function deleteButton(pos, resources, redraw) {
        let delButton = delButtonTpl.content.firstChild.cloneNode(true);
        delButton.onclick = () => {
            resources.splice(pos, 1);
            redraw();
        };
        return delButton;
    }

    function drawPlaylist(tagId, rootElem, resources, collapsed, previousTitleElem, previousTracksElem) {
        let tracksElem = playlistList(collapsed);
        let titleElem;

        const redraw = () => {
            drawPlaylist(tagId, rootElem, resources, false, titleElem, tracksElem);
        };

        titleElem = playlistTitle(tagId, tagId, tracksElem, resources, redraw);

        if (previousTitleElem)
            rootElem.replaceChild(titleElem, previousTitleElem);
        else
            rootElem.appendChild(titleElem);

        if (previousTracksElem)
            rootElem.replaceChild(tracksElem, previousTracksElem);
        else
            rootElem.appendChild(tracksElem);

        if (resources !== null) {
            for (let i = 0; i < resources.length; i++) {
                tracksElem.appendChild(playlistItem(i, resources, redraw));
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
                    originalResources[tagId] = [...resources];
                }
                drawPlaylist(tagId, rootElem, resources, true);
            });
        },
        close: nop
    };
}
