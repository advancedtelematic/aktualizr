
install(DIRECTORY schemas
        DESTINATION lib/sota
        COMPONENT aktualizr)

install(FILES systemd/aktualizr-ubuntu.service
        DESTINATION lib/systemd/system RENAME aktualizr.service COMPONENT aktualizr)
install(FILES sota_ubuntu.toml
        DESTINATION lib/sota RENAME sota.toml COMPONENT aktualizr)
install(FILES secondary/virtualsec.json
        DESTINATION lib/sota RENAME demo_secondary.json COMPONENT aktualizr)
