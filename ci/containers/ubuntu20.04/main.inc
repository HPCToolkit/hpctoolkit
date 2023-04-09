# Main Containerfile fragment for Ubuntu Focal (20.04)

# Add the deadsnakes PPA
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
  && DEBIAN_FRONTEND=noninteractive apt-get install --yes --no-install-recommends software-properties-common \
  && DEBIAN_FRONTEND=noninteractive add-apt-repository --yes --no-update ppa:deadsnakes/ppa

# Install all the packages required:
#  - Base system pieces required by Spack
#  - Compilers
#  - Extra utilities
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
  && DEBIAN_FRONTEND=noninteractive apt-get install --yes --no-install-recommends \
    bzip2 \
    ca-certificates \
    ccache \
    clang libomp-dev \
    curl \
    eatmydata \
    file \
    gcc-10 g++-10 gfortran-10 \
    gcc-9 g++-9 gfortran-9 \
    gcc g++ gfortran \
    git \
    git-lfs \
    gnupg2 \
    make \
    mercurial \
    patch \
    patchelf \
    python3.10-full \
    subversion \
    unzip \
    xz-utils \
    zstd \
  && rm -rf /var/lib/apt/lists/* \
  && ln -s /usr/bin/python3.10 /usr/local/bin/python3 \
  && python3 -m ensurepip --upgrade

# Install the permanent Spack configuration
COPY config.yaml /etc/spack/

INCLUDE ../spack.inc
