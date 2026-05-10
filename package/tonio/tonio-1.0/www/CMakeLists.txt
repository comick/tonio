# Install web assets to ${CMAKE_INSTALL_DATADIR}/tonio/www
set(TONIO_WWW_FILES
    index.html
    tonio.css
    tonio.js
    lib.js
    log.js
    common.js
    settings.js
    setup.js
    status.js
)

install(FILES ${TONIO_WWW_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/tonio/www
)
