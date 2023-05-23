# This is a modified version of the Dockerfile template from the official Spack template:
# https://github.com/spack/spack/blob/c2ce9a6d930891c708d822be8ce22aa5f12a58ee/share/spack/templates/container/Dockerfile
# Unfortunately that template is missing pieces we need. Rather than hack after the Dockerfile is
# written, the adjustments are made in this copy here. Specifically:
#  - spack install has --no-check-signature added,
#  - A custom mirrors.yaml is mounted from the secrets in the spack install RUN command, and
#  - The ENTRYPOINT is rewritten to actually work when you pass arguments containing spaces.

{% if render_phase.bootstrap %}
{{ bootstrap.recipe }}

{% endif %}
{% if render_phase.build %}
# Build stage with Spack pre-installed and ready to be used
FROM {{ build.image }} as builder

{% block build_stage %}
{% if os_packages_build %}
# Install OS packages needed to build the software
RUN {% if os_package_update %}{{ os_packages_build.update }} \
 && {% endif %}{{ os_packages_build.install }} {{ os_packages_build.list | join | replace('\n', ' ') }} \
 && {{ os_packages_build.clean }}
{% endif %}

# What we want to install and how we want to install it
# is specified in a manifest file (spack.yaml)
RUN mkdir {{ paths.environment }} \
{{ manifest }} > {{ paths.environment }}/spack.yaml

# Install the software, remove unnecessary deps
RUN --mount=type=secret,id=spack_mirrors,target=/root/.spack/mirrors.yaml \
{% if depfile %}
  cd {{ paths.environment }} && spack env activate . && spack concretize && spack env depfile -o Makefile && make -j $(nproc) && spack gc -y
{% else %}
  cd {{ paths.environment }} && spack env activate . && spack install --no-check-signature --fail-fast && spack gc -y
{% endif %}
{% if strip %}

# Strip all the binaries
RUN find -L {{ paths.view }}/* -type f -exec readlink -f '{}' \; | \
    xargs file -i | \
    grep 'charset=binary' | \
    grep 'x-executable\|x-archive\|x-sharedlib' | \
    awk -F: '{print $1}' | xargs strip
{% endif %}

# Modifications to the environment that are necessary to run
RUN spack env activate --sh -d {{ paths.environment }} > /opt/spack-environment/activate.sh

# The view is a symlink. Copy it into a separate directory to preserve it's symlink status.
RUN mkdir /view-final && mv {{ paths.view }} /view-final/

{% if extra_instructions.build %}
{{ extra_instructions.build }}
{% endif %}
{% endblock build_stage %}
{% endif %}

{% if render_phase.final %}
# Bare OS image to run the installed executables
FROM {{ run.image }}

{% if os_packages_final %}
RUN {% if os_package_update %}{{ os_packages_final.update }} \
 && {% endif %}{{ os_packages_final.install }} {{ os_packages_final.list | join | replace('\n', ' ') }} \
 && {{ os_packages_final.clean }}
{% endif %}

COPY --from=builder {{ paths.environment }} {{ paths.environment }}
COPY --from=builder {{ paths.store }} {{ paths.store }}
COPY --from=builder {{ paths.hidden_view }} {{ paths.hidden_view }}
COPY --from=builder /view-final /opt

RUN ln -s /opt/spack-environment/activate.sh /etc/profile.d/z10_spack_environment.sh \
&&  printf '#!/bin/sh\n. /opt/spack-environment/activate.sh\nexec "$@"' > /entrypoint.sh \
&&  chmod a+x /entrypoint.sh

{% block final_stage %}
{% if extra_instructions.final %}
{{ extra_instructions.final }}
{% endif %}
{% endblock final_stage %}
{% for label, value in labels.items() %}
LABEL "{{ label }}"="{{ value }}"
{% endfor %}
ENTRYPOINT ["/entrypoint.sh"]
CMD [ "/bin/bash" ]
{% endif %}
