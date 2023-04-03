# Containerfile fragment to install Spack and all the relevant bits

# Install Spack (custom branch until upstream support is implemented)
ENV SPACK_ROOT=/opt/spack
RUN git clone --depth=5 --single-branch --no-tags --branch=octo-all-ci-improvements \
      https://github.com/blue42u/spack.git $SPACK_ROOT \
  && rm -r $SPACK_ROOT/.git/ \
  && ln -s $SPACK_ROOT/bin/spack /usr/bin/spack

INCLUDE spack-bootstrap.inc

# Install the Spack-built dependencies based on the concrete environments.
# We do the same sequence for each environment, just with different paths,
# in order from slowest-changing to fastest to encourage cache hits.
# We always restore the environment's spack.yaml after installing to dodge Spack bugs.

# /minimum/: Minimum supported versions of dependencies
COPY --from=envs minimum/spack.yaml /spack/cenv/minimum/
COPY --from=cenvs minimum/spack.lock /spack/cenv/minimum/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
    --mount=type=cache,target=/root/.spack-cache/ \
  $SPACK_ROOT/bin/spack --env /spack/cenv/minimum install --use-buildcache=only --no-check-signature --fail-fast
COPY --from=envs minimum/spack.yaml /spack/cenv/minimum/

# /latest/: Latest available versions of dependencies
COPY --from=envs latest/spack.yaml /spack/cenv/latest/
COPY --from=cenvs latest/spack.lock /spack/cenv/latest/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
    --mount=type=cache,target=/root/.spack-cache/ \
  $SPACK_ROOT/bin/spack --env /spack/cenv/latest install --use-buildcache=only --no-check-signature --fail-fast
COPY --from=envs latest/spack.yaml /spack/cenv/latest/
