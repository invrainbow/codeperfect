#!/bin/bash

cd /root/codeperfect
git pull
cd gostuff
go build -o bin/api github.com/invrainbow/codeperfect/gostuff/cmd/api
sudo service api restart
