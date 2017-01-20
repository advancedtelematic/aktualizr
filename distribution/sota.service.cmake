[Unit]
Description=Sota Client
After=network.target

[Service]
Type=simple
ExecStart=${CMAKE_INSTALL_PREFIX}/bin/sota_client -c /etc/sota.conf
Restart=always

[Install]
WantedBy=multi-user.target
