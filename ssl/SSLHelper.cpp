#include <fcntl.h>
#include <unistd.h>
#include "ssl/SSLHelper.h"
#include "ssl/SSLContext.h"
#include "ssl/CertCache.h"
#include "core/Logging.h"

namespace tigerso {

#define CERT_KEY_LENGTH 2048

X509* loadX509FromFile(const char* filename) {
    
    BIO* bio = NULL;

    bio = BIO_new(BIO_s_file());
    if(BIO_read_filename(bio, filename) <= 0) {
        BIO_free(bio);
        return NULL;
    }

    X509* x = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return x;
}

int storeX509ToPEMStr(X509* cert, char* buf, int len) {

    BIO* bio = NULL;
    int ret = 0;

    bio = BIO_new(BIO_s_mem());
    if(bio == NULL) {
        return -1;
    }

    if(!PEM_write_bio_X509(bio, cert)) {
        BIO_free(bio);
        return -1;
    }

    if( 0 > (ret = BIO_read(bio, (void*)buf, len))) {
        BIO_free(bio);
        return ret;
    }

    BIO_free(bio);
    return ret;
}

EVP_PKEY* loadPrivateKeyFromFile(const char* keyfile, const char* passwd) {

    EVP_PKEY* pkey = NULL;
    BIO* bio = NULL;

    bio = BIO_new(BIO_s_file());

    if(BIO_read_filename(bio, keyfile) <= 0) {
        BIO_free(bio);
        return NULL;
    }

    pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, const_cast<char*>(passwd));
    if(pkey == NULL) {
        INFO_LOG("read private key failed");
        BIO_free(bio);
        return NULL;
    }

    BIO_free(bio);
    return pkey;
}

static bool genSerialNumber(char* issuer, char* commonName, char* serial, char* newSerial, int len) {

    if(NULL == issuer || NULL == commonName || NULL == serial || 0 > len) {
        return false;
    }

    char buf[1924];
    char md5value[128];
    snprintf(buf, sizeof(buf), "%s.%s.%s.tigerso", issuer, serial, commonName);
    SSLHelper::MD5(buf,md5value, sizeof(md5value));
    snprintf(newSerial, len, "0x%s", md5value);
    return true;
}

static RSA* genRSA(int key_length) {

    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();

    if(BN_set_word(bn, 0x10001) <=0 || RSA_generate_key_ex(rsa, key_length, bn, NULL) <= 0) {
       BN_free(bn);
       RSA_free(rsa);
       return NULL;
    }
    return rsa;
}

bool SSLHelper::signCert(X509* ca_cert, EVP_PKEY* ca_pkey, int key_length, X509* org_cert, X509** cert, EVP_PKEY** pkey) { 
    
    if(ca_cert == NULL || ca_pkey == NULL) {
        INFO_LOG("Invalid input parameter");
        return false;
    }

    RSA* new_cert_rsa;
    *cert = NULL;
    *pkey = NULL;

    char issuer[256];
    X509_NAME* xissuer = X509_get_issuer_name(org_cert);
    X509_NAME_oneline(xissuer, issuer, sizeof(issuer));

    char commonName[512];
    X509_NAME *subj = X509_get_subject_name(org_cert);
    X509_NAME_get_text_by_NID(subj, NID_commonName, commonName, sizeof(commonName));

    ASN1_INTEGER *org_serial = X509_get_serialNumber(org_cert);
    char serialStr[64] = {0};
    for(int i = 0; i < org_serial->length; i++) {
        snprintf(serialStr + 2 * i, 3, "%02x", org_serial->data[i]);
    }
    
    char serialNumber[128];
    genSerialNumber(issuer, commonName, serialStr, serialNumber, sizeof(serialNumber));
    DBG_LOG("generate new serial number: %s", serialNumber);

    ASN1_INTEGER * serial = s2i_ASN1_INTEGER(NULL, serialNumber);
    if(serial == NULL) {
        INFO_LOG("get serial failed");
        return false;
    }

    new_cert_rsa = genRSA(key_length);
    if(new_cert_rsa == NULL) {
        ASN1_INTEGER_free(serial);
        INFO_LOG("gen RSA failed");
        return false;
    }

    *pkey = EVP_PKEY_new();
    if(*pkey == NULL) {
        RSA_free(new_cert_rsa);
        ASN1_INTEGER_free(serial);
        INFO_LOG("new private key failed");
        return false;
    }

    if(EVP_PKEY_set1_RSA(*pkey, new_cert_rsa) < 0) {
        EVP_PKEY_free(*pkey);
        RSA_free(new_cert_rsa);
        ASN1_INTEGER_free(serial);
        pkey == NULL;
        INFO_LOG("set private key from RSA failed");
        return false;
    }

    RSA_free(new_cert_rsa);

    int extcount = 0;
    const char* extstr;
    X509_EXTENSION* extension;
    bool subjectAltName = false;

    if((extcount = X509_get_ext_count(org_cert)) > 0) {
        for(int i = 0; i < extcount; i++) {
            extension = X509_get_ext(org_cert, i);
            extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(extension)));
            if(!strcmp(extstr, "subjectAltName")) {
                subjectAltName = true;
                break;
            }
        }
    }

    *cert = X509_new();
    if(*cert == NULL) {
        ASN1_INTEGER_free(serial);
        EVP_PKEY_free(*pkey);
        *pkey = NULL;
        INFO_LOG("new cert failed");
        return false;
    }

    if(subjectAltName) {
        X509_add_ext(*cert, extension, -1);
    }

    if(X509_set_pubkey(*cert, *pkey) <= 0 ||
        X509_set_subject_name(*cert, X509_get_subject_name(org_cert)) <= 0 ||
        X509_set_version(*cert, X509_get_version(ca_cert)) <= 0 ||
        X509_set_issuer_name(*cert, X509_get_subject_name(ca_cert)) <= 0 ||
        X509_set_notBefore(*cert, X509_get_notBefore(ca_cert)) <= 0 ||
        X509_set_notAfter(*cert, X509_get_notAfter(ca_cert)) <= 0 ||
        X509_set_serialNumber(*cert, serial) <= 0) {
        
        ASN1_INTEGER_free(serial);
        EVP_PKEY_free(*pkey);
        *pkey = NULL;

        X509_free(*cert);
        *cert = NULL;
        INFO_LOG("set cert values failed");
        return false;
    }

    ASN1_INTEGER_free(serial);
    if(X509_sign(*cert, ca_pkey, EVP_sha256()) <= 0) {
        EVP_PKEY_free(*pkey);
        *pkey = NULL;

        X509_free(*cert);
        *cert = NULL;
        INFO_LOG("sign cert failed");
        return false;
    }

    return true;
}

bool SSLHelper::validSSL(SSL* ssl) {
    if(NULL == ssl || SSL_get_fd(ssl) < 0) { return false; }
    return true;
}

int SSLHelper::MD5(const char*  input, char* output, int len) {
    if(input == NULL || output == NULL || len < 0) {
		return 1;
	}

	unsigned char digest[17];
	char message[1024];
	char buf[33];

	snprintf(message, sizeof(message) - 1, "%s", input);
	message[sizeof(message) - 1] = '\0';

	MD5_CTX    mctx;
	MD5_Init(&mctx);
	MD5_Update(&mctx, (unsigned char*) message, strlen(message));
	MD5_Final(digest, &mctx);

	char *p = buf, *tail = buf + sizeof(buf);
	for (int i = 0; i < 16; i++) {
		p += snprintf(p, tail - p, "%2.2hx", digest[i]);
	}

	strncpy(output, (const char*) buf, len - 1);
	output[len - 1] = '\0';
	return 0;
}

int SSLHelper::resignContextCert(SSLContext& client, SSLContext& server) {
    X509* org_cert = client._peerCert;
    if(!org_cert) {
       client._peerCert = SSL_get_peer_certificate(client._ssl);
       org_cert = client._peerCert;
       if(!org_cert) {
            INFO_LOG("no peer certificate presented in SSL");
            return -1;
        }
    }

    CertCache* pcache = CertCache::getInstance();
    X509* cert = NULL;
    EVP_PKEY* pkey = NULL;
    int ret = pcache->query(org_cert, &cert, &pkey);

    //resign certificate and store in certcache
    if(ret != 0) {
        DBG_LOG("no resigned cert in CertCache, resign it");
        X509* cacert = NULL;
        EVP_PKEY* capkey = NULL;
        ret = SSLContext::getCAEntity(&cacert, &capkey); 
        if(ret < 0) { return -1; }
        if(!signCert(cacert, capkey, CERT_KEY_LENGTH, org_cert, &cert, &pkey)) {
            INFO_LOG("failed to resign certificate");
            return -1;
        }

        ret = pcache->store(org_cert, cert, pkey);
        if(ret != 0) {
            INFO_LOG("failed to store certificate into certcache");
            return -1;
        }
    }

    server._ownCert = cert;
    server._ownPkey= pkey;
    INFO_LOG("Https Decryption: Success");
    return 0;
}

}//namespace tigerso
