{% extends "spins/Dockerfile" %}
{% block final_stage %}
{{ super() }}

COPY dev.json /opt/spack-environment/

# Install Spack develop
ENV SPACK_ROOT=/opt/spack
RUN git clone --depth=5 --single-branch --no-tags --branch=develop \
      https://github.com/spack/spack.git $SPACK_ROOT \
  && ln -s $SPACK_ROOT/bin/spack /usr/bin/spack \
  && spack clean --bootstrap && spack bootstrap now

{% endblock %}
