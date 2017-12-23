#!/usr/bin/env python

from gzip import GzipFile
from StringIO import StringIO
from base64 import b64encode
from textwrap import wrap
#from argparse import ArgumentParser
import urllib2
import sys
import re
import socket

_TMP_CA_PEM_DIR="./tmp_capem/"

def fetchUrl(url):
	'''
	fetchUrl(url) -> bytes

	Gets the contents of a URL, performing GZip decoding if needed
	'''

	request = urllib2.Request(url)
	request.add_header('Accept-Encoding', 'gzip')
	try:
		sock = urllib2.urlopen(request)
		data = sock.read()

		# Check for GZip encoding and decode if needed
		if (sock.headers.get('content-encoding', None) == 'gzip'):
			data = GzipFile(fileobj=StringIO(data)).read()

		return data
	except urllib2.HTTPError, e:
		print e.code
		return None


def parseNSSFile(explicitTrustOnly = True, trustServerAuth = True,
	trustEmailProtection = False, trustCodeSigning = False):
	'''
	parseNSSFile(explicitTrustOnly = True, trustServerAuth = True,
		trustEmailProtection = False, trustCodeSigning = False) -> tuple

	Downloads and parses out the license, date, and trusted certificate roots
	contained in the NSS certificate root file.
	'''
	objs = []
	currObj = None
	license = None
	date = None

	#content = fetchUrl('http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt?raw=1')
	content = fetchUrl('http://mxr.mozilla.org/mozilla-central/source/security/nss/lib/ckfw/builtins/certdata.txt?raw=1')
	if (content == None):
		return None

	lines = content.splitlines()

	for (i, line) in enumerate(lines):
		if (-1 != line.find('This Source Code Form')):
			license = getLicense(lines, i)

		if (line.startswith('CVS_ID')):
			date = re.findall('\$Date: (.*) \$', line)[0]

		if (not line.startswith('CKA')):
			continue

		words = line.split(' ', 2)

		if ("CKA_CLASS" == words[0]):
			if (None != currObj):
				objs.append(currObj)

			currObj = {}

		if ('MULTILINE_OCTAL' == words[1]):
			value = parseMultlineOctal(lines, i + 1)
		else:
			value = words[2]

		currObj[words[0]] = {'type' : words[1], 'value' : value}

	certs = (x for x in objs if x['CKA_CLASS']['value'] == 'CKO_CERTIFICATE')
	trusts = [x for x in objs if x['CKA_CLASS']['value'] == 'CKO_NSS_TRUST']

	trustedCerts = []
	for cert in certs:
		if	(isCertTrusted(cert, trusts, explicitTrustOnly, trustServerAuth,
				trustEmailProtection, trustCodeSigning)):
			trustedCerts.append(cert)
	print trustedCerts
	print license
	print date

	return (license, date, trustedCerts)

def parseMultlineOctal(lines, num):
	'''
	parseMultlineOctal(lines, num) -> list(number)

	Parses out a MULTILINE_OCTAL block and returns a list of the byte contents
	'''
	value = []

	while True:
		if ('END' == lines[num]):
			# Convert octal digits into ints using map/lambda
			return map(lambda x: int(x, 8), value)
		elif (lines[num].startswith('\\')):
			# Skip the first item as its empty
			value.extend(lines[num].split('\\')[1:])
		else:
			return None
		num += 1

def getLicense(lines, num):
	'''
	getLicense(lines, num) -> string

	Parses out the license and returns it as a single string
	'''
	license = []

	while True:
		if (lines[num].startswith('# certdata.txt')):
			break
		license.append(lines[num][2:])

		num += 1

	return ' '.join(license)

def isCertTrusted(cert, trusts, explicitTrustOnly, trustServerAuth,
		trustEmailProtection, trustCodeSigning):
	'''
	isCertTrusted(cert, trusts, explicitTrustOnly, trustServerAuth,
		trustEmailProtection, trustCodeSigning) -> boolean

	Examines a certificate and evaluates it against the trust objects and
	criteria to determine if it should be trusted
	'''
	issuer = cert['CKA_ISSUER']['value']
	serial = cert['CKA_SERIAL_NUMBER']['value']

	for trust in trusts:
		if ((trust['CKA_ISSUER']['value'] == issuer) and (trust['CKA_SERIAL_NUMBER']['value'] == serial)):
			if (trustServerAuth):
				if (	(trust['CKA_TRUST_SERVER_AUTH']['value'] == 'CKT_NSS_TRUSTED_DELEGATOR')
						or (	(trust['CKA_TRUST_SERVER_AUTH']['value'] == 'CKT_NSS_MUST_VERIFY_TRUST')
							and (not explicitTrustOnly)
						)
					):
						return True

			if (trustEmailProtection):
				if (	(trust['CKA_TRUST_EMAIL_PROTECTION']['value'] == 'CKT_NSS_TRUSTED_DELEGATOR')
						or (	(trust['CKA_TRUST_EMAIL_PROTECTION']['value'] == 'CKT_NSS_MUST_VERIFY_TRUST')
							and (not explicitTrustOnly)
						)
					):
						return True

			if (trustCodeSigning):
				if (	(trust['CKA_TRUST_CODE_SIGNING']['value'] == 'CKT_NSS_TRUSTED_DELEGATOR')
					or (	(trust['CKA_TRUST_CODE_SIGNING']['value'] == 'CKT_NSS_MUST_VERIFY_TRUST')
						and (not explicitTrustOnly)
					)
				):
					return True

	else:
		return False

def main(explicitTrustOnly = True, trustServerAuth = True,
	trustEmailProtection = False, trustCodeSigning = False):
	'''
	main(explicitTrustOnly = True, trustServerAuth = True,
		trustEmailProtection = False, trustCodeSigning = False)

	Downloads and parses the NSS certificate root file, writing out the license,
	data, and trusted certificates to a PEM file
	'''
	parsedNSS = parseNSSFile(explicitTrustOnly,
					trustServerAuth,
					trustEmailProtection,
					trustCodeSigning)

	if (None == parsedNSS):
		sys.stderr.write("Could not find any certificates\n")
		return

	i = 0
	for trustedCert in parsedNSS[2]:
		i += 1
		f = open(_TMP_CA_PEM_DIR + str(i) + ".pem", 'wt')
		f.write("-----BEGIN CERTIFICATE-----\n")
		f.write("\n".join(
			wrapB64(
				b64encode(''.join(map(chr, trustedCert['CKA_VALUE']['value']))),
				76)
			) + "\n")
		f.write("-----END CERTIFICATE-----\n\n")
		f.close()

def wrapB64(str, width = 70):
	'''
	wrapB64(str[, width = 70]) -> list(str)

	Wraps a string that was base-64 encoded to a set width.  textwrap.wrap
	is extremely slow at contiguous strings
	'''
	offset = 0
	retVal = []
	strLen = len(str)

	while True:
		line = str[offset : offset + width]
		retVal.append(line)

		if ((offset + width) >= strLen):
			break

		offset += width

	return retVal

if (__name__ == "__main__"):
	main(True, True, False, False)
