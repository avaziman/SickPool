#!/bin/bash  

scripts=$(<./redis_scripts.lua)
echo $scripts
redis-cli FUNCTION LOAD LUA sickpool REPLACE "$scripts"