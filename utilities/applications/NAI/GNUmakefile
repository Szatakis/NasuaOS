# NAI application module. The .napp image is built with the shared app.mk
# rules; this makefile additionally verifies the bundled model weights before
# they are allowed into the operating system images.
#
# Optional payloads provided by this module:
#   payload/   installed into the rootfs at /share/licenses/nai/
#   iso/       installed into iso_root at /system/NAI/
# Both are handled generically by the parent makefiles; payload files whose
# symlink targets are absent (e.g. missing model weights) are skipped.

APP := NAI
MODEL := model/SmolLM2-135M-Instruct-IQ4_XS.gguf
MODEL_SIZE := 90897760
MODEL_SHA256 := d766661c2e39c3da78d8eddf15287af263dce1eedc1c9c8093ed8e54eb067dbc

.PHONY: all verify-model clean distclean

all: verify-model
	$(MAKE) -f ../app.mk APP=$(APP)

verify-model:
	@if [ -f $(MODEL) ]; then \
		actual_size=$$(wc -c < $(MODEL) | tr -d '[:space:]'); \
		actual_sha256=$$(sha256sum $(MODEL) | cut -d ' ' -f 1); \
		if [ "$$actual_size" != "$(MODEL_SIZE)" ] || \
		   [ "$$actual_sha256" != "$(MODEL_SHA256)" ]; then \
			printf '%s\n' "NAI model size or SHA-256 mismatch; refusing to package it." >&2; \
			exit 1; \
		fi; \
	else \
		printf '%s\n' "NAI model is absent; the bootable images will omit NAI weights."; \
	fi

clean:
	$(MAKE) -f ../app.mk APP=$(APP) clean

distclean:
	$(MAKE) -f ../app.mk APP=$(APP) distclean
