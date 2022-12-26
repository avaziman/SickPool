apt install sudo
adduser sickpool

# close all ports the ones except necessary to be open
#sudo apt install iptables
sudo apt install ufw
sudo ufw reset
sudo ufw allow ssh
sudo ufw allow https
sudo ufw allow 4444/tcp
sudo ufw default deny incoming
sudo ufw enable

sudo ufw status verbose
sudo ufw show raw


# general use
sudo apt install curl wget rsync tmux

# web server
sudo apt install nginx

# cert bot
sudo apt install snapd
sudo snap install core
sudo snap refresh core
sudo snap install --classic certbot
sudo ln -s /snap/bin/certbot /usr/bin/certbot
sudo certbot --nginx

# install dependencies
sudo apt install libhiredis-dev libssl-dev libc6 libc6-dev gpg libssl3 libssl-dev libmysqlcppconn7v5

# redis
curl -fsSL https://packages.redis.io/gpg | sudo gpg --dearmor -o /usr/share/keyrings/redis-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/redis.list

sudo apt-get update
sudo apt-get install redis
# redis end

# redis timeseries
git clone --recursive https://github.com/RedisTimeSeries/RedisTimeSeries.git
cd RedisTimeSeries
make setup
make build

# mysql
wget https://dev.mysql.com/get/mysql-apt-config_0.8.24-1_all.deb
sudo apt install ./mysql-apt-config_0.8.24-1_all.deb
sudo apt update
sudo apt install mysql-server
 
# move to the location of our custom services
sudo cp ./$STRATUM_SERVICE.service /etc/systemd/system
sudo cp ./$API_SERVICE.service /etc/systemd/system

#!/bin/bash
STRATUM_SERVICE=sickpool_stratum
API_SERVICE=sickpool_api

# reload the services and enable them to run on boot and immediately
sudo systemctl daemon-reload
sudo systemctl enable $STRATUM_SERVICE --now 
sudo systemctl enable $API_SERVICE --now

# Zano
wget https://build.zano.org/builds/zano-linux-x64-release-devtools-v1.5.0.143%5B336fac2%5D.tar.bz2
tar -xf 'zano-linux-x64-release-devtools-v1.5.0.143[336fac2].tar.bz2'
