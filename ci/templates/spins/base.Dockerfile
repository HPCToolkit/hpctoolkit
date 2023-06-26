{% extends "spins/Dockerfile" %}
{% block build_stage %}
{{ super() }}

# Save the compilers to copy into the final image
RUN spack config get compilers > /compilers.yaml

{% endblock %}
{% block final_stage %}
{{ super() }}

COPY --from=builder /compilers.yaml /etc/spack/compilers.yaml
COPY dev.json /opt/spack-environment/

# Install Spack develop
ENV SPACK_ROOT=/opt/spack
RUN git clone --depth=5 --single-branch --no-tags --branch=develop \
      https://github.com/spack/spack.git $SPACK_ROOT \
  && ln -s $SPACK_ROOT/bin/spack /usr/bin/spack \
  && spack clean --bootstrap && spack bootstrap now

ENV HPCTOOLKIT_DEV_SYSTEM_PYTHON=1

{% endblock %}
