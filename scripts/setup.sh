#!/bin/bash
STRATUM_SERVICE=sickpool_stratum
API_SERVICE=sickpool_api

# move to the location of our custom services
sudo mv ./$STRATUM_SERVICE.service /etc/systemd/system
sudo mv ./$API_SERVICE.service /etc/systemd/system

# reload the services and enable them to run on boot and immediately
sudo systemctl daemon-reload
sudo systemctl enable $STRATUM_SERVICE --now 
sudo systemctl enable $API_SERVICE --now