SPACK ?= spack

.PHONY: all

all: post-all

include spack.mk

ifeq ($(REMOTE_CACHE_MIRROR),)
$(warning No remote-cache mirror specified, will not be using the remote cache)
post-all: $(addprefix predeps/install/,$(predeps/SPACK_PACKAGE_IDS))
else
predeps/post/%: predeps/install/%
	-flock . $(SPACK) -D . buildcache push --unsigned --fail-fast -j $(shell nproc) $(REMOTE_CACHE_MIRROR) /$(HASH)

post-all: $(addprefix predeps/post/,$(predeps/SPACK_PACKAGE_IDS))
	-$(SPACK) -D . buildcache push --unsigned --fail-fast -j $(shell nproc) $(REMOTE_CACHE_MIRROR)
	-$(SPACK) -D . buildcache update-index $(REMOTE_CACHE_MIRROR)
endif
