/**
 * Copyright (c) 2020-2023 Michele Comignano <mcdev@playlinux.net>
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
window.onload = async () => {
    window.stage = (() => {
        let currentStageId;

        const stages = {
            status: statusStageCreate(),
            library: libraryStageCreate(),
            log: logStageCreate(),
            settings: settingsStageCreate(),
            setup: setupStageCreate(),
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

            if (currentStageId !== 'setup') {
                document.getElementById('menu').style = 'display: block';
            }

            let currentRootElem = document.getElementById(currentStageId + '-stage');
            stages[currentStageId].open(currentRootElem);
            currentRootElem.style = 'display: block';
            window.location.hash = '#' + currentStageId;
            document.getElementById(currentStageId + '-href').classList.toggle('active');
            return true;
        };

    })();

    let settings = await(await fetch('/settings')).json();
    if (settings.factory_new) {
        stage('setup');
    } else {
        let hash = window.location.hash.substr(1);
        (hash && hash !== 'setup' && stage(hash)) || stage('status');
    }
};
