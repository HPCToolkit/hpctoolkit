#!/bin/bash -e

# Install important packages that may be missing on the current system.
# Usage: ensure-present.sh <programs>...

# shellcheck disable=SC1091
os_code="$(source /etc/os-release && echo "$ID:$VERSION_ID")"

# Test each "program" and see what's missing
declare -A missing_sys missing_py
for prog in "$@"; do
  case "$prog" in
  # The default C/C++ compiler for any OS is simply termed cc.
  cc)
    case "$os_code" in
    ubuntu:20.04)
      command -v gcc-9 >/dev/null || missing_sys+=([gcc-9]=1)
      command -v g++-9 >/dev/null || missing_sys+=([g++-9]=1)
      ;;
    ubuntu:*)
      echo "Unrecognized Debian-based OS: $os_code"
      exit 1
      ;;

    *)
      command -v gcc >/dev/null || missing_sys+=([gcc]=1)
      command -v g++ >/dev/null || missing_sys+=([g++]=1)
      ;;
    esac
    ;;

  # Python packages are prefixed with py:
  # We only support Python 3.10, assume everything is missing if we don't have that
  py:*)
    package=$(echo "$prog" | cut -d: -f2-)
    if command -v python3 >/dev/null \
       && python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
      python3 -c "import $package" &>/dev/null || missing_py+=(["$package"]=1)
    else
      missing_sys+=([python3]=1)
      missing_py+=(["$prog"]=1)
    fi
    ;;

  python3)
    if ! command -v python3 >/dev/null \
       || ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
      missing_sys+=([python3]=1)
    fi
    ;;

  *)
    command -v "$prog" >/dev/null || missing_sys+=(["$prog"]=1)
    ;;
  esac
done
if [ "${#missing_sys[@]}" -eq 0 ] && [ "${#missing_py[@]}" -eq 0 ]; then
  echo "All packages already installed!"
  exit 0
fi

### PHASE 1: System-level dependencies

# Install missing packages that will be supplied by the OS itself.
case "$os_code" in
ubuntu:20.04)
  declare -A apt_packages
  for prog in "${!missing_sys[@]}"; do
    case "$prog" in
    py:*)
      # Handled by the Pip code later
      ;;
    python3)
      # Use the deadsnakes PPA to get up-to-date versions of Python 3.
      # See https://launchpad.net/~deadsnakes/+archive/ubuntu/ppa
      echo "deb https://ppa.launchpadcontent.net/deadsnakes/ppa/ubuntu focal main" > /etc/apt/sources.list.d/deadsnakes.list
      apt-key adv --keyserver keyserver.ubuntu.com --recv-keys F23C5A6CF475977595C89F51BA6932366A755776
      apt_packages+=([python3.10]=1 [python3.10-venv]=1)
      ln -sf /usr/bin/python3.10 /usr/local/bin/python3
      ;;
    git|gcc-9|g++-9|ccache|file|patchelf|curl|make|eatmydata)
      apt_packages+=(["$prog"]=1)
      ;;
    *)
      echo "Unrecognized program: $prog"
      exit 1
      ;;
    esac
  done

  if [ "${#apt_packages[@]}" -gt 0 ]; then
    echo -e "\e[0Ksection_start:$(date +%s):apt_install[collapsed=true]\r\e[0Kapt-get install ${!apt_packages[*]}"
    rm -f /etc/apt/apt.conf.d/docker-clean
    mkdir -pv .apt-cache/
    apt-get update -yq
    DEBIAN_FRONTEND=noninteractive apt-get -o Dir::Cache::Archives=".apt-cache/" install -y "${!apt_packages[@]}"
    echo -e "\e[0Ksection_end:$(date +%s):apt_install\r\e[0K"
  fi
  ;;

almalinux:8.*|fedora:36)
  declare -A yum_packages
  for prog in "${!missing_sys[@]}"; do
    case "$prog" in
    py:*)
      # Handled by the Pip code later
      ;;
    eatmydata)
      yum_packages+=(["$prog"]=1)
      ;;
    *)
      echo "Unrecognized program: $prog"
      exit 1
      ;;
    esac
  done

  if [ "${#yum_packages[@]}" -gt 0 ]; then
    echo -e "\e[0Ksection_start:$(date +%s):yum_install[collapsed=true]\r\e[0Kyum install ${!yum_packages[*]}"
    yum install -y "${!yum_packages[@]}"
    echo -e "\e[0Ksection_end:$(date +%s):yum_install\r\e[0K"
  fi
  ;;

opensuse-leap:15.*)
  declare -A zypper_packages
  for prog in "${!missing_sys[@]}"; do
    case "$prog" in
    py:*)
      # Handled by the Pip code later
      ;;
    eatmydata)
      zypper_packages+=(["$prog"]=1)
      ;;
    *)
      echo "Unrecognized program: $prog"
      exit 1
      ;;
    esac
  done

  if [ "${#zypper_packages[@]}" -gt 0 ]; then
    echo -e "\e[0Ksection_start:$(date +%s):zypper_install[collapsed=true]\r\e[0Kzypper install ${!zypper_packages[*]}"
    zypper install -y "${!zypper_packages[@]}"
    echo -e "\e[0Ksection_end:$(date +%s):zypper_install\r\e[0K"
  fi
  ;;

*)
  echo "Unsupported OS: $os_code"
  exit 1
  ;;
esac

### PHASE 2: Python dependencies

# If Python was needed in phase 1, we need to refresh missing_py with new information.
if [ "${missing_sys[python3]}" ]; then
  hash python3
  missing_py=()
  for prog in "$@"; do
    case "$prog" in
    # Python packages are prefixed with py:
    py:*)
      package=$(echo "$prog" | cut -d: -f2-)
      python3 -c "import $package" &>/dev/null || missing_py+=(["$package"]=1)
      ;;
    esac
  done
fi

# Install missing packages that will be supplied by Pip
declare -A pip_packages
for package in "${!missing_py[@]}"; do
  case "$package" in
  boto3|clingo)
    pip_packages+=(["$package"]=1)
    ;;

  *)
    echo "Unrecognized py:package: $package"
    exit 1
    ;;
  esac
done
if [ "${#pip_packages[@]}" -gt 0 ]; then
  echo -e "\e[0Ksection_start:$(date +%s):pip_install[collapsed=true]\r\e[0Kpip install ${!pip_packages[*]}"
  mkdir -pv .pip-cache/
  python3 -m ensurepip --upgrade
  python3 -m pip --cache-dir .pip-cache/ install --upgrade pip
  python3 -m pip --cache-dir .pip-cache/ install "${!pip_packages[@]}"
  echo -e "\e[0Ksection_end:$(date +%s):pip_install\r\e[0K"
fi
