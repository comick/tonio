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

function logStageCreate() {
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
}
