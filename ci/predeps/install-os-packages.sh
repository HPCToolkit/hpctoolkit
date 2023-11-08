#!/bin/bash -e

# Source os-release to get the information about the current OS
# shellcheck disable=SC1091
. /etc/os-release


# Collect the set of packages to install. Most packages have the same name across OSs, but some,
# especially compilers, are more finicky.
PACKAGES=(
  ccache
  diffutils
  diffutils
  eatmydata
  file
  git git-lfs
  tar
)

case "$ID" in
almalinux|rhel)
  case "$VERSION_ID" in
  8.*)
    # The default Python in RHEL 8 is 3.6, which is too old for modern Meson. Install 3.9 instead.
    PACKAGES+=(python39 python39-pip)
    ;;
  *)
    # Use the default Python in later/other RHELs.
    PACKAGES+=(python3 python3-pip)
    ;;
  esac
  ;;

opensuse-leap)
  case "$VERSION_ID" in
  15.*)
    # The default Python in Leap 15 is 3.6, which is too old for modern Meson. Install 3.11 instead.
    PACKAGES+=(python311 python311-pip)
    ;;
  *)
    # Use the default Python in later/other Leaps.
    PACKAGES+=(python3 python3-pip)
    ;;
  esac
  ;;

*)
  # In all other cases, either the default Python suffices, or no suitable Python is available.
  PACKAGES+=(python3 python3-pip)
  ;;
esac

case "$ID" in
almalinux|rhel|centos|fedora)
  # The Red Hat OSs all only ever ship a single version of the compilers.
  PACKAGES+=(
    gcc
    gcc-c++
    clang
  )
  ;;

ubuntu)
  case "$VERSION_ID" in
  20.04)
    PACKAGES+=(
      gcc gcc-8 gcc-9 gcc-10
      g++ g++-8 g++-9 g++-10
      clang clang-9 clang-10 clang-11
      libomp-dev
    )
    ;;
  22.04)
    PACKAGES+=(
      gcc gcc-9 gcc-10 gcc-11 gcc-12
      g++ g++-9 g++-10 g++-11 g++-12
      clang clang-11 clang-14
      libomp-dev
    )
    ;;
  *)
    echo "Unsupported version of Ubuntu: $VERSION_ID" >&2
    exit 1
    ;;
  esac
  ;;

opensuse-leap)
  case "$VERSION_ID" in
  15.*)
    PACKAGES+=(
      gcc gcc8 gcc9 gcc10 gcc11 gcc12
      gcc-c++ gcc8-c++ gcc9-c++ gcc10-c++ gcc11-c++ gcc12-c++
      clang clang9 clang11 clang13 clang14 clang15
    )
    ;;
  *)
    echo "Unsupported version of OpenSUSE Leap: $VERSION_ID" >&2
    exit 1
    ;;
  esac
  ;;

*)
  echo "Unsupported OS: $ID" >&2
  exit 1
  ;;
esac

# For Intel images specifically, we want to go a step further and install IGC.
if [ -d /usr/include/level_zero ]; then
  PACKAGES+=(libigc-dev libigdfcl-dev)
fi


# Now actually install all the packages we want, and clean up afterwards.
case "$ID" in
almalinux|rhel)
  # We will need epel-release to install some packages that aren't technically part of RHEL.
  dnf install -y epel-release
  ;&
fedora)
  dnf install -y "${PACKAGES[@]}"
  dnf clean all
  ;;

ubuntu)
  apt-get update -yqq
  apt-get install -yqq "${PACKAGES[@]}"
  apt-get clean
  rm -rf /var/lib/apt/lists/*
  ;;

opensuse-leap)
  zypper install -y "${PACKAGES[@]}"
  zypper clean
  ;;
esac


# For Intel images specifically, we want to go a step further and install GTPin. Unfortunately
# GTPin is only available via a separate download and not via packages. So we download and manually.
if [ -d /usr/include/level_zero ]; then
  curl -L -o /tmp/gtpin-3.4.tar.xz https://downloadmirror.intel.com/777295/external-release-gtpin-3.4-linux.tar.xz
  sha256sum --check - <<'EOF'
c96d08a2729c255e3bc67372fc1271ba60ca8d7bd913f92c2bd951d7d348f553 /tmp/gtpin-3.4.tar.xz
EOF
  mkdir -p /opt/gtpin-3.4
  tar -C /opt/gtpin-3.4 -xJf /tmp/gtpin-3.4.tar.xz
  rm /tmp/gtpin-3.4.tar.xz
fi
