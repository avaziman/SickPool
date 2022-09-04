#!/bin/bash
STRATUM_SERVICE=sickpool_stratum
API_SERVICE=sickpool_api

# move to the location of our custom services
sudo mv ./$STRATUM_SERVICE.service /etc/systemd/system
sudo mv ./$API_SERVICE.service /etc/systemd/system

# install building dependencies
# server
sudo apt install libhiredis-dev gcc-10
# api
curl https://sh.rustup.rs -sSf | sh
# website
sudo apt install node

# redis
curl -fsSL https://packages.redis.io/gpg | sudo gpg --dearmor -o /usr/share/keyrings/redis-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/redis.list

sudo apt-get update
sudo apt-get install redis

# reload the services and enable them to run on boot and immediately
sudo systemctl daemon-reload
sudo systemctl enable $STRATUM_SERVICE --now 
sudo systemctl enable $API_SERVICE --now