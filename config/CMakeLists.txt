if(BUILD_DEB)
    install(FILES systemd/aktualizr-ubuntu.service
        DESTINATION lib/systemd/system
        RENAME aktualizr.service
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)

    install(FILES sota_ubuntu.toml
        DESTINATION lib/sota/conf.d
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)

    install(FILES secondary/virtualsec.json
        DESTINATION lib/sota/secondaries
        RENAME demo_secondary.json
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)
endif(BUILD_DEB)

install(DIRECTORY DESTINATION lib/sota/conf.d COMPONENT aktualizr)
