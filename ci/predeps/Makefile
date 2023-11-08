SPACK ?= spack
REMOTE_CACHE_MIRROR := __ci_remote_cache_push_mirror

.PHONY: all

all: post-all

include spack.mk

ifeq ($(shell spack mirror list | grep -c '^$(REMOTE_CACHE_MIRROR)'),0)
$(warning Remote cache mirror $(REMOTE_CACHE_MIRROR) not found, will not be pushing to the remote cache)
post-all: $(addprefix predeps/install/,$(predeps/SPACK_PACKAGE_IDS))
else
predeps/post/%: predeps/install/%
	-flock . $(SPACK) -D . buildcache push --unsigned --fail-fast -j $(shell nproc) $(REMOTE_CACHE_MIRROR) /$(HASH)

post-all: $(addprefix predeps/post/,$(predeps/SPACK_PACKAGE_IDS))
	-$(SPACK) -D . buildcache push --unsigned --fail-fast -j $(shell nproc) $(REMOTE_CACHE_MIRROR)
	-$(SPACK) -D . buildcache update-index $(REMOTE_CACHE_MIRROR)
endif
