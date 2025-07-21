# Copyright 2014 Ayla Networks, Inc.  All rights reserved.

QUIET ?= @
CERTS_DEST ?= .
NULL :=
SHELL = /bin/bash

.PHONY: default
default: der pem

.PHONY: der
der:	ca_certs_der.txt
.PHONY: pem
pem:	ca_certs_pem.txt

.PHONY: der_c
der_c:	$(CERTS_DEST)/ca_certs_der_txt.c

.PHONY: der_t
der_t:	$(CERTS_DEST)/ca_certs_der.txt

#
# The checked-in files are the .pem certs.
#
CERTS_PEM = \
	entrust-ca-g2-sha256-2048.pem \
	entrust-ca-sha1-2048-sn-0x456b5054.pem \
	valicert-cl2-sha1-1024.pem \
	Baltimore_CyberTrust_Root.pem \
	$(NULL)

CERTS_DER = $(CERTS_PEM:%.pem=%.der)

ca_certs_der.txt: $(CERTS_DER)
	$(QUIET)cat $^ > $@

#
# Generating PEM certs with NUL-termination for esp_mqtt.
#
ca_certs_pem.txt: $(CERTS_PEM)
	$(QUIET)(cat $^; echo -ne '\x00' ) > $@

#
# Generate copyright lines for .c files.
#
define CA_COPYRIGHT
	echo '/*'; \
	echo ' * Copyright ' \
	    $(shell date +%Y) \
	    ' Ayla Networks, Inc.  All rights reserved.'; \
	echo ' */'
endef

#
# Rule for PEM or DER certs concatenated in C
#
$(CERTS_DEST)/%_txt.c: %.txt
	$(QUIET)( \
		$(call CA_COPYRIGHT); \
		xxd -i $< \
	) > $@

$(CERTS_DEST)/%.txt: %.txt
	$(QUIET)cp "$<" "$@"

#
# DER certs with individual certs described in a struct.
# These are used by the Realtek port.
#
ca_certs_der.c: $(CERTS_DER)
	@( \
		$(call CA_COPYRIGHT); \
		for cert in \
			$(CERTS_DER); \
		do \
			var=$${cert%*.der}; \
			var=$${var//-/_}; \
			var=$${var:0:32}; \
			echo "/*"; \
			echo " * $$cert"; ( \
				openssl x509 -in $$cert -inform der -noout -startdate -enddate | awk -F= \
					'{print " * "$$1":";print " *    "$$2;}' \
			); \
			echo "*/"; \
			echo "static const unsigned char $$var[] = {"; \
			hexdump -e '12/1 "0x%02x, " "\n"' $$cert | sed -e s/^/\ \ / -e s/\ 0x\ \ ,//g; \
			echo "};"; \
		done; \
		echo "const struct"; \
		echo "{"; \
		echo "	const char *name;"; \
		echo "	const unsigned char *cert;"; \
		echo "	size_t size;"; \
		echo "} ca_certs_der[] = {"; \
		for cert in $(CERTS_DER); do \
	  		var=$${cert%*.der}; \
	  		var=$${var//-/_}; \
	  		var=$${var:0:32}; \
	  		echo "  {"; \
	  		echo "    \"$$cert\","; \
	  		echo "    $$var,"; \
	  		echo "    sizeof($$var)"; \
	  		echo "  },"; \
		done; \
		echo "	{ NULL, NULL, 0 },"; \
		echo "};"; \
	) > $@

%.der: %.pem
	$(QUIET)openssl x509 -in $< -out $@ -inform pem -outform der

#
# Target to display the certs on stdout
#
text: $(CERTS_DER)
	$(QUIET)for cert in $(CERTS_DER); do \
		echo; echo $$cert; \
		openssl x509 -in $$cert -noout -inform der -text; \
	done

clean:
	rm -f $(CERTS_DER) ca_certs_pem.txt ca_certs_der.txt ca_certs_der.c \
		ca_certs_pem_txt.c ca_certs_der_txt.c
