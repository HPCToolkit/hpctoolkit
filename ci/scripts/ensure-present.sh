#!/bin/bash -e

# Install important packages that may be missing on the current system.
# Usage: ensure-present.sh <programs>...

os_code="$(grep ^ID= /etc/os-release | cut -d= -f2-):$(grep ^VERSION_ID= /etc/os-release | cut -d= -f2- | tr -d '"')"

# Test each "program" and see what's missing
missing=()
for prog in "$@"; do
  case "$prog" in
  # The default C/C++ compiler for any OS is simply termed cc.
  cc)
    case "$os_code" in
    ubuntu:20.04)
      command -v gcc-9 >/dev/null || missing+=(gcc-9)
      command -v g++-9 >/dev/null || missing+=(g++-9)
      ;;
    ubuntu:*)
      echo "Unrecognized Debian-based OS: $os_code"
      exit 1
      ;;

    *)
      command -v gcc >/dev/null || missing+=(gcc)
      command -v g++ >/dev/null || missing+=(g++)
      ;;
    esac
    ;;

  # Python packages are prefixed with py:
  # We only support Python 3.10, assume everything is missing if we don't have that
  py:*)
    if command -v python3 >/dev/null \
       && python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
      python3 -c "import $(echo "$prog" | cut -d: -f2-)" &>/dev/null || missing+=("$prog")
    else
      missing+=(python3 "$prog")
    fi
    ;;

  python3)
    if ! command -v python3 >/dev/null \
       || ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
      missing+=(python3)
    fi
    ;;

  *)
    command -v "$prog" >/dev/null || missing+=("$prog")
    ;;
  esac
done
if [ "${#missing[@]}" -eq 0 ]; then
  echo "All packages already installed!"
  exit 0
fi


bundles=()


# Install missing packages that will be supplied by the OS itself.
case "$os_code" in
ubuntu:20.04)
  apt_packages=()
  for prog in "${missing[@]}"; do
    case "$prog" in
    py:*)
      # Handled by the Pip code later
      ;;
    python3)
      # Ubuntu 20.04 ships Python 3.8, we want something newer. So use the bundle instead.
      bundles+=(python.tar.xz)
      ;;
    git|gcc-9|g++-9|ccache|file|patchelf|curl|make)
      apt_packages+=("$prog")
      ;;
    *)
      echo "Unrecognized program: $prog"
      exit 1
      ;;
    esac
  done

  if [ "${#apt_packages[@]}" -gt 0 ]; then
    echo -e "\e[0Ksection_start:$(date +%s):apt_install[collapsed=true]\r\e[0Kapt-get install ${apt_packages[*]}"
    rm -f /etc/apt/apt.conf.d/docker-clean
    mkdir -pv .apt-cache/
    apt-get update -yq
    DEBIAN_FRONTEND=noninteractive apt-get -o Dir::Cache::Archives=".apt-cache/" install -y "${apt_packages[@]}"
    echo -e "\e[0Ksection_end:$(date +%s):apt_install\r\e[0K"
  fi
  ;;

*)
  echo "Unsupported OS: $os_code"
  exit 1
  ;;
esac


# Install any missing packages that will be supplied by "bundles" overlaid on top of the OS
if [ "${#bundles[@]}" -gt 0 ]; then
  echo -e "\e[0Ksection_start:$(date +%s):bundle_install[collapsed=true]\r\e[0Ktar xaf ${bundles[*]}"
  for bundle in "${bundles[@]}"; do
    curl --no-progress-meter -L -o /tmp/"$bundle" http://bundleserver/"$bundle"
    tar -C / -xaf /tmp/"$bundle"
    rm /tmp/"$bundle"
  done
  hash -r
  echo -e "\e[0Ksection_end:$(date +%s):bundle_install\r\e[0K"
fi


# Install missing packages that will be supplied by Pip
pip_packages=()
for prog in "${missing[@]}"; do
  case "$prog" in
  py:boto3|py:clingo)
    pip_packages+=("$(echo "$prog" | cut -d: -f2-)")
    ;;

  py:*)
    echo "Unrecognized py:package: $prog"
    exit 1
    ;;
  esac
done
if [ "${#pip_packages[@]}" -gt 0 ]; then
  echo -e "\e[0Ksection_start:$(date +%s):pip_install[collapsed=true]\r\e[0Kpip install ${pip_packages[*]}"
  mkdir -pv .pip-cache/
  python3 -m pip --cache-dir .pip-cache/ install --upgrade pip
  python3 -m pip --cache-dir .pip-cache/ install "${pip_packages[@]}"
  echo -e "\e[0Ksection_end:$(date +%s):pip_install\r\e[0K"
fi
