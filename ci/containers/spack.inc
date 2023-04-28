# Containerfile fragment to install Spack and all the relevant bits

# Install Spack develop
ENV SPACK_ROOT=/opt/spack
RUN git clone --depth=5 --single-branch --no-tags --branch=develop \
      https://github.com/spack/spack.git $SPACK_ROOT \
  && ln -s $SPACK_ROOT/bin/spack /usr/bin/spack

INCLUDE spack-bootstrap.inc

# Install the Spack-built dependencies based on the concrete environments.
# We do the same sequence for each environment, just with different paths,
# in order from slowest-changing to fastest to encourage cache hits.

# We also git pull the Spack in each case, to ensure that if the environment
# *did* update we get an updated Spack, instead of whatever stale thing might
# be in the layer cache.

# /_autogen/: Standard tools used to form the Autogoo stashed in the repository
COPY --from=cenv _autogen/spack.yaml _autogen/spack.lock /spack/cenv/_autogen/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
  git -C $SPACK_ROOT pull --ff-only \
  && $SPACK_ROOT/bin/spack --env /spack/cenv/_autogen install --use-buildcache=only --no-check-signature --fail-fast
COPY --from=cenv _autogen/spack.yaml /spack/cenv/_autogen/

# /minimum/: Minimum supported versions of dependencies
COPY --from=cenv minimum/spack.yaml minimum/spack.lock minimum/dev.json /spack/cenv/minimum/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
  git -C $SPACK_ROOT pull --ff-only \
  && $SPACK_ROOT/bin/spack --env /spack/cenv/minimum install --use-buildcache=only --no-check-signature --fail-fast
COPY --from=cenv minimum/spack.yaml /spack/cenv/minimum/

# /latest/: Latest available versions of dependencies
COPY --from=cenv latest/spack.yaml latest/spack.lock latest/dev.json /spack/cenv/latest/
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
  git -C $SPACK_ROOT pull --ff-only \
  && $SPACK_ROOT/bin/spack --env /spack/cenv/latest install --use-buildcache=only --no-check-signature --fail-fast
COPY --from=cenv latest/spack.yaml /spack/cenv/latest/
