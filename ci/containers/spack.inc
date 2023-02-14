# Containerfile fragment to install Spack and all the relevant bits

# Install Spack (custom branch until upstream support is implemented)
ENV SPACK_ROOT=/opt/spack
RUN git clone --depth=5 --single-branch --no-tags --branch=octo-all-ci-improvements \
      https://github.com/blue42u/spack.git $SPACK_ROOT \
  && rm -r $SPACK_ROOT/.git/ \
  && ln -s $SPACK_ROOT/bin/spack /usr/bin/spack

INCLUDE spack-bootstrap.inc

# Install the Spack-built dependencies based on the concrete environment(s)
COPY --from=envs latest/spack.yaml /spack/cenv/latest/
COPY --from=cenvs latest/spack.lock /spack/cenv/latest/
COPY --from=envs minimum/spack.yaml /spack/cenv/minimum/
COPY --from=cenvs minimum/spack.lock /spack/cenv/minimum/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
    --mount=type=secret,id=buildcache_config,target=/root/.spack/config.yaml \
    --mount=type=cache,from=buildcache_cache,target=/cache/spack-bincache-shared \
    --mount=type=cache,from=buildcache_cache_nonprot,target=/cache/spack-bincache-nonprotected \
  $SPACK_ROOT/bin/spack --env /spack/cenv/latest install --use-buildcache=only --no-check-signature --fail-fast \
  && $SPACK_ROOT/bin/spack --env /spack/cenv/minimum install --use-buildcache=only --no-check-signature --fail-fast
# Re-copy the spack.yaml files to dodge Spack's bad habit of altering them
COPY --from=envs latest/spack.yaml /spack/cenv/latest/
COPY --from=envs minimum/spack.yaml /spack/cenv/minimum/
