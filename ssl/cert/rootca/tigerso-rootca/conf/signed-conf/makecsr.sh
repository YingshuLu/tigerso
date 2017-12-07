#!/bin/sh
openssl req -config openssl-tigerso.conf -newkey rsa:2048 -sha256 -nodes -out tigerso.csr -outform PEM
