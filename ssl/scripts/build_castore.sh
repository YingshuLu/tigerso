#!/bin/sh

HOME=".."
_MOZILLA_CADATA_URL="https://curl.haxx.se/ca/cacert.pem"
_HASH_CA_PEM_DIR=${HOME}/../ssl/cert/trustCA/hash_PEM/ca
_RAW_PEM_STORE=${_HASH_CA_PEM_DIR}
_TMP_PEM_FILE=./tmp_capem

_DOWNLOAD_SCRIPT=./ca_converter.py

clear_oldPEM () {
    rm -rf ${_HASH_CA_PEM_DIR}/*
}

hash_caPEM() {
    pemfile=$1
    is_CA=`openssl x509 -inform PEM -in $pemfile -purpose -noout | grep "Any Purpose CA" | awk -F" : " '{ print $2 }'`
    if [ "$is_CA" != "Yes" ]
    then
        echo "$pemfile is not a CA certificate"
        return -1
    fi
    hash=`openssl x509 -hash -noout -in $pemfile`
    if [ -h $rehash_d/${hash}.0 ]
    then
        index=`ls $rehash_d/${hash}.* | wc -w`
    else
        index=0
    fi
    mv $pemfile $_HASH_CA_PEM_DIR/${hash}.${index}
    return 0
}

#__main__

clear_oldPEM

#${_DOWNLOAD_SCRIPT}

for file in ${_TMP_PEM_FILE}/*.pem
do
    hash_caPEM $file
done


