# Main Containerfile fragment for Alma Linux 8 (proxy for RHEL 8)

# Install all the packages required:
#  - Base system pieces required by Spack
#  - Compilers
#  - Extra utilities
RUN yum install -y epel-release shadow-utils \
  && yum update -y \
  && yum install -y \
    bzip2 \
    ccache \
    clang \
    diffutils \
    eatmydata \
    file \
    gcc gcc-c++ gcc-gfortran \
    git \
    git-lfs \
    make \
    mercurial \
    patch \
    patchelf \
    perl-open \
    python3 \
    python3-devel \
    subversion \
    tar \
    unzip \
    xz \
    zstd \
  && yum clean all && rm -rf /var/yum/* /var/dnf/*

# Install the permanent Spack configuration
COPY config.yaml /etc/spack/

INCLUDE ../spack.inc

# Install a Python 3.10 with Spack to overwrite the older system one
ADD https://bootstrap.pypa.io/get-pip.py /tmp/get-pip.py
RUN --mount=type=secret,id=buildcache,target=/root/.spack/mirrors.yaml \
    --mount=type=cache,target=/root/.spack-cache/ \
  $SPACK_ROOT/bin/spack install --fail-fast --no-check-signature python@3.10 \
  && cp -s $($SPACK_ROOT/bin/spack find --format '{prefix}' python@3.10 | head -n1)/bin/* /usr/local/bin/ \
  && yum remove -y python3 && yum clean all && rm -rf /var/yum/* /var/dnf/* \
  && python3 /tmp/get-pip.py && rm /tmp/get-pip.py

INCLUDE ../spack-bootstrap.inc
INCLUDE ../finalize.inc
