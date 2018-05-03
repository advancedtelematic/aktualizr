
install(DIRECTORY schemas
        DESTINATION lib/sota
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        COMPONENT aktualizr)

install(FILES systemd/aktualizr-ubuntu.service
        DESTINATION lib/systemd/system
        RENAME aktualizr.service
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)

install(FILES sota_ubuntu.toml
        DESTINATION lib/sota
        RENAME sota.toml
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)

install(FILES secondary/virtualsec.json
        DESTINATION lib/sota
        RENAME demo_secondary.json
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT aktualizr)
