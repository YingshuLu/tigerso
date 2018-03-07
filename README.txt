Tigerso is a HTTP/HTTPS server frame.

It mainly has two function:
    
    1. HTTP/HTTPS Server:
        a. static plain text (least for 1.0.0 version), directly support Jekyll etc
        b. comments post, save & display 
        c. user authenication, attack actions detection

    2. HTTP/HTTPS proxy
        a. HTTPS decryption using provided CA certicate
        b. simple user ACL policy
        c. URL filter, only whitelist/blacklist (will fully introduce functions like squidGuard)
        d. true filetype detection online
        e. notification page delivery for voilate actions
        f. target image replace online, i.e. big GIF image replaced with a small image.

features would be introduced:
    a. online file anti-virus scan, integrated with libclamav.
    b. online malicious javascript detection, integrated with my JSE (javascript scan engine) which is based on machine learning.


Install steps:

1. Http(s) server:

    $ cd test
    $ make -f Makefile.server

by default, server listens on 80 & 443 port with default end certificate.

2. Http(s) server:

    $ cd test
    $ make -f Makefile.proxy

If Https decryption is enabled, client browser should import tigerso/customized CA certification into CA trust list.

