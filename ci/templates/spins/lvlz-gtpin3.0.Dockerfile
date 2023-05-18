{% extends "spins/base.Dockerfile" %}
{% block final_stage %}

# Wipe the problematic environment set by the basekit image
ENV PYTHONPATH=
ENV PKG_CONFIG_PATH=
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV CPATH=
ENV LIBRARY_PATH=
ENV LD_LIBRARY_PATH=
ENV CMAKE_PREFIX_PATH=

# Install GTPin
ADD https://downloadmirror.intel.com/730598/external-release-gtpin-3.0-linux.tar.xz /tmp/gtpin-3.0.tar.xz
RUN mkdir /opt/gtpin && tar -C /opt/gtpin/ -xJf /tmp/gtpin-3.0.tar.xz && rm /tmp/gtpin-3.0.tar.xz

{{ super() }}
{% endblock %}
