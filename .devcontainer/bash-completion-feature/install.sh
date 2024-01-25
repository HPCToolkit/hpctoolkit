#!/bin/sh

# Install the bash completion package
if ! [ -f /etc/bash_completion ]; then
  if command -v apt-get; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -yqq
    apt-get install -y bash-completion
    rm -rf /var/apt/lists/*
  elif command -v dnf; then
    dnf install -y bash-completion
    dnf clean all
  else
    echo "Unsupported distro"
    exit 2
  fi
fi
