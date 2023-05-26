{% extends "spins/base.Dockerfile" %}
{% block final_stage %}

# Dockerfile for CI image for Alma Linux 8 (proxy for RHEL 8) with CUDA 10.2
# Derived from the Dockerfiles used for Nvidia official images for RHEL UBI 8:
# https://gitlab.com/nvidia/container-images/cuda/-/blob/master/dist/10.2/ubi8/
ARG TARGETARCH

# This particular buildscript only works on x86_64
RUN test "$TARGETARCH" = "amd64"

# =====================================================
# https://gitlab.com/nvidia/container-images/cuda/-/blob/a07cb97de5798b75147cbe1158c6caadffee2892/dist/10.2/ubi8/base/Dockerfile
# =====================================================

ENV NVARCH x86_64
ENV NVIDIA_REQUIRE_CUDA cuda>=10.2 brand=tesla,driver>=418,driver<419
ENV NV_CUDA_CUDART_VERSION 10.2.89-1

COPY cuda.repo.amd64 /etc/yum.repos.d/cuda.repo

#LABEL maintainer "NVIDIA CORPORATION <sw-cuda-installer@nvidia.com>"

RUN NVIDIA_GPGKEY_SUM=d0664fbbdb8c32356d45de36c5984617217b2d0bef41b93ccecd326ba3b80c87 && \
    curl -fsSL https://developer.download.nvidia.com/compute/cuda/repos/rhel8/${NVARCH}/D42D0685.pub | sed '/^Version/d' > /etc/pki/rpm-gpg/RPM-GPG-KEY-NVIDIA && \
    echo "$NVIDIA_GPGKEY_SUM  /etc/pki/rpm-gpg/RPM-GPG-KEY-NVIDIA" | sha256sum -c --strict  -

ENV CUDA_VERSION 10.2

# For libraries in the cuda-compat-* package: https://docs.nvidia.com/cuda/eula/index.html#attachment-a
RUN yum upgrade -y && yum install -y \
    cuda-cudart-10-2-${NV_CUDA_CUDART_VERSION} \
    cuda-compat-10-2 \
    && ln -s cuda-10.2 /usr/local/cuda \
    && yum clean all \
    && rm -rf /var/cache/yum/*

# nvidia-docker 1.0
RUN echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf && \
    echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf

ENV PATH /usr/local/nvidia/bin:/usr/local/cuda/bin:${PATH}
ENV LD_LIBRARY_PATH /usr/local/nvidia/lib:/usr/local/nvidia/lib64

#COPY NGC-DL-CONTAINER-LICENSE /

# nvidia-container-runtime
ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility


# =====================================================
# https://gitlab.com/nvidia/container-images/cuda/-/blob/a07cb97de5798b75147cbe1158c6caadffee2892/dist/10.2/ubi8/runtime/Dockerfile
# =====================================================

ENV NV_CUDA_LIB_VERSION 10.2.89-1
ENV NV_NVTX_VERSION 10.2.89-1
ENV NV_LIBNPP_VERSION 10.2.89-1

ENV NV_LIBCUBLAS_PACKAGE_NAME libcublas10
ENV NV_LIBCUBLAS_VERSION 10.2.2.89-1
ENV NV_LIBCUBLAS_PACKAGE ${NV_LIBCUBLAS_PACKAGE_NAME}-${NV_LIBCUBLAS_VERSION}

ENV NV_LIBNCCL_PACKAGE_NAME libnccl
ENV NV_LIBNCCL_PACKAGE_VERSION 2.13.4-1
ENV NCCL_VERSION 2.13.4
ENV NV_LIBNCCL_PACKAGE ${NV_LIBNCCL_PACKAGE_NAME}-${NV_LIBNCCL_PACKAGE_VERSION}+cuda10.2

#LABEL maintainer "NVIDIA CORPORATION <sw-cuda-installer@nvidia.com>"

RUN yum install -y \
    cuda-libraries-10-2-${NV_CUDA_LIB_VERSION} \
    cuda-nvtx-10-2-${NV_NVTX_VERSION} \
    cuda-npp-10-2-${NV_LIBNPP_VERSION} \
    ${NV_LIBCUBLAS_PACKAGE} \
    && yum clean all \
    && rm -rf /var/cache/yum/*


# =====================================================
# https://gitlab.com/nvidia/container-images/cuda/-/blob/a07cb97de5798b75147cbe1158c6caadffee2892/dist/10.2/ubi8/devel/Dockerfile
# =====================================================

ENV NV_CUDA_LIB_VERSION 10.2.89-1
ENV NV_NVPROF_VERSION 10.2.89-1
ENV NV_CUDA_CUDART_DEV_VERSION 10.2.89-1
ENV NV_NVML_DEV_VERSION 10.2.89-1

ENV NV_LIBNPP_DEV_VERSION 10.2.89-1

ENV NV_LIBCUBLAS_DEV_PACKAGE_NAME libcublas-devel
ENV NV_LIBCUBLAS_DEV_VERSION 10.2.2.89-1
ENV NV_LIBCUBLAS_DEV_PACKAGE ${NV_LIBCUBLAS_DEV_PACKAGE_NAME}-${NV_LIBCUBLAS_DEV_VERSION}

ENV NV_LIBNCCL_DEV_PACKAGE_NAME libnccl-devel
ENV NV_LIBNCCL_DEV_PACKAGE_VERSION 2.13.4-1
ENV NCCL_VERSION 2.13.4
ENV NV_LIBNCCL_DEV_PACKAGE ${NV_LIBNCCL_DEV_PACKAGE_NAME}-${NV_LIBNCCL_DEV_PACKAGE_VERSION}+cuda10.2


#LABEL maintainer "NVIDIA CORPORATION <sw-cuda-installer@nvidia.com>"


RUN yum install -y \
    make \
    cuda-nvml-dev-10-2-${NV_NVML_DEV_VERSION} \
    cuda-command-line-tools-10-2-${NV_CUDA_LIB_VERSION} \
    cuda-cudart-dev-10-2-${NV_CUDA_CUDART_DEV_VERSION} \
    cuda-libraries-dev-10-2-${NV_CUDA_LIB_VERSION} \
    cuda-minimal-build-10-2-${NV_CUDA_LIB_VERSION} \
    cuda-nvprof-10-2-${NV_NVPROF_VERSION} \
    cuda-npp-dev-10-2-${NV_LIBNPP_DEV_VERSION} \
    ${NV_LIBCUBLAS_DEV_PACKAGE} \
    ${NV_LIBNCCL_DEV_PACKAGE} \
    && yum clean all \
    && rm -rf /var/cache/yum/*

ENV LIBRARY_PATH /usr/local/cuda/lib64/stubs

{{ super() }}
{% endblock %}
