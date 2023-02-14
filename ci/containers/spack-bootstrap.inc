# Containerfile fragment to bootstrap a Spack installation on a fresh Python

# Spack knows how to bootstrap most things for itself
# boto3 is required for AWS S3 access
RUN spack clean --bootstrap \
  && spack bootstrap now \
  && python3 -m pip install --no-cache-dir boto3
