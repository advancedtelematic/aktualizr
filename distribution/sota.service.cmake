[Unit]
Description=Aktualizr SOTA Client
After=network.target

[Service]
Type=simple
ExecStart=${CMAKE_INSTALL_PREFIX}/bin/aktualizr -c /etc/sota.conf
Restart=always

[Install]
WantedBy=multi-user.target
