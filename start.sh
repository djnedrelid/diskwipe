#!/bin/sh

#
#   Dobbeltklikk og velg "Run in terminal".
#   Eller $ sudo ./start.sh fra terminal.
#

# Advarsel før start.
read -p "sda er fredet, sdb-z vil bli automatisk slettet! (trykk enter for å fortsette)" advarsel

# Starter diskliste programmet i egen terminal som scanner og sletter disker.
cd ~/Desktop/diskwipe/cpphelpers

# Under utvikling.
sudo gnome-terminal -- ./diskliste

# SystemRescue release.
#xfce4-terminal -x ./diskliste

# Starter lokal webserver som leser logger fra diskliste programmet.
cd ~/Desktop/diskwipe
python3 ./server.py

# Bruk deretter WebUI for oversikt.
