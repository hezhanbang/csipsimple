#!/bin/sh

rm -rf CSipSimple.apk 
java -jar signed/signapk.jar signed/platform.x509.pem signed/platform.pk8 ../CSipSimple-release-unsigned.apk CSipSimple.apk