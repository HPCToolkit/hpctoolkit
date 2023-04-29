# Containerfile fragment to finalize the image after all system deps are installed

# Install the ./dev scriptsuite, and populate the environments generated earlier
COPY --from=lib dev /src/dev
RUN python3 -m venv --upgrade-deps --clear /opt/dev_venv \
  && /opt/dev_venv/bin/pip install --no-cache-dir --require-virtualenv /src/dev && rm -r /src/dev \
  && /opt/dev_venv/bin/dev populate /spack/cenv/minimum \
  && /opt/dev_venv/bin/dev populate /spack/cenv/latest
