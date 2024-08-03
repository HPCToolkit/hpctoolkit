# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

# Download and convert signing keys for the extra repositories
FROM docker.io/alpine as keys
ADD https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1604/x86_64/3bf863cc.pub \
  /usr/share/keyrings/cuda.asc
ADD https://repo.radeon.com/rocm/rocm.gpg.key /usr/share/keyrings/rocm.asc
ADD https://repositories.intel.com/gpu/intel-graphics.key /usr/share/keyrings/intel-graphics.asc
ADD https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
  /usr/share/keyrings/intel-oneapi.asc
RUN \
  apk add gpg \
  && for f in /usr/share/keyrings/*.asc; do \
    gpg --yes --dearmor --output /usr/share/keyrings/"$(basename "$f" .asc)".gpg < "$f" \
    && rm "$f" || exit $?; \
  done

FROM docker.io/ubuntu:24.04

# Install certificates first. These are needed to use https sources for the extra repositories.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update -y \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates

# Copy in the extra repository configurations and keyring
COPY apt /etc/apt
COPY --from=keys /usr/share/keyrings /usr/share/keyrings

# Install the packages. Packages from the extra repositories are only installed if available for
# the architecture in question.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update -y \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ccache \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    curl \
    g++ \
    gcc \
    git \
    latexmk \
    libboost-atomic-dev \
    libboost-chrono-dev \
    libboost-date-time-dev \
    libboost-filesystem-dev \
    libboost-graph-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-timer-dev \
    libbz2-dev \
    libdw-dev \
    libelf-dev \
    libiberty-dev \
    liblzma-dev \
    libomp-dev \
    libpapi-dev \
    libpfm4-dev \
    libtbb-dev \
    libunwind-dev \
    libxerces-c-dev \
    libxxhash-dev \
    libyaml-cpp-dev \
    libzstd-dev \
    make  `# FIXME: See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/704` \
    mawk  `# FIXME: See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/704` \
    mpi-default-dev \
    ninja-build \
    ocl-icd-opencl-dev \
    pkg-config \
    python3 \
    python3-dev \
    python3-docutils \
    python3-myst-parser \
    python3-pip \
    python3-sphinx \
    python3-venv \
    sed  `# FIXME: See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/704` \
    tex-gyre \
    texlive \
    texlive-latex-extra \
    texlive-plain-generic \
    zlib1g-dev \
    $({ \
      case "$(dpkg --print-architecture)" in \
      amd64|arm64) echo 'cuda-cupti-dev-12-6 cuda-compiler-12-6 cuda-opencl-dev-12-6 cuda-nvdisasm-12-6'; ;; \
      esac; \
    }) \
    $({ \
      case "$(dpkg --print-architecture)" in \
      amd64) echo 'rocm-dev'; ;; \
      esac; \
    }) \
    $({ \
      case "$(dpkg --print-architecture)" in \
      amd64) echo 'intel-oneapi-compiler-dpcpp-cpp libze-dev libigdfcl-dev libigc-dev libigfxcmrt-dev'; ;; \
      esac; \
    }) \
  && : # END

# Set up pipx. Pipx is useful to install Python-based utilities without messing up the system.
RUN python3 -m pip install --break-system-packages pipx && pipx ensurepath --global

# Install the latest version of Meson
RUN pipx install --global 'meson>=1.1.0,<2'

# Add CUDA, ROCm, and some parts of Intel's OneAPI to the default search paths
ENV \
  PATH="$PATH:/usr/local/cuda/bin:/opt/rocm/bin:/opt/intel/oneapi/compiler/latest/bin" \
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/opt/intel/oneapi/compiler/latest/lib" \
  CMAKE_PREFIX_PATH="/opt/rocm"

# Generate a suitable Meson native file for all the features this image has.
RUN mkdir -p /usr/share/meson/native \
  && t=/usr/share/meson/native/hpctoolkit-all-features.ini && rm -f "$t" \
  && echo "[project options]" >> "$t" \
  && echo "auto_features = 'enabled'" >> "$t" \
  && echo "manual_pdf = 'enabled'" >> "$t" \
  && if ! [ -e /usr/local/cuda ]; then echo "cuda = 'disabled'" >> "$t"; fi \
  && if ! [ -e /opt/rocm ]; then echo "rocm = 'disabled'" >> "$t"; fi \
  && if ! [ -e /opt/intel ]; then \
    echo "level0 = 'disabled'" >> "$t" \
    && echo "gtpin = 'disabled'" >> "$t"; \
  fi \
  && : # EOF

# Extend the set above with settings used for CI testing
RUN t=/usr/share/meson/native/hpctoolkit-ci.ini \
  && cp /usr/share/meson/native/hpctoolkit-all-features.ini "$t" \
  && echo "[built-in options]" >> "$t" \
  && echo "wrap_mode = 'nofallback'" >> "$t" \
  && echo "force_fallback_for = ['dyninst', 'xed', 'gtpin']" >> "$t" \
  && : # EOF
