#!/bin/sh
openssl ca -config openssl-ca.conf -policy signing_policy -extensions signing_req -out tigerso-ca-cert.pem -infiles ./signed-conf/tigerso.csr

