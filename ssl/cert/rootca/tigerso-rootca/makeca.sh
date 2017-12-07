#!/bin/sh

openssl req -x509 -config ./conf/openssl-ca.conf -newkey rsa:4096 -sha256 -nodes -out cacert.pem -outform PEM 
