FROM ubuntu:20.04

# Set DEBIAN_FRONTEND to noninteractive to automate package installation
ARG DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    gdb \
    valgrind \
    kcachegrind \
    make \
    linux-tools-common \
    linux-tools-generic \
    sysstat \
    && apt-get install -y linux-tools-`uname -r` || true \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /usr/src/lsm_tree

# Copy your local project files into the Docker container
COPY . /usr/src/lsm_tree/

# Default command to execute when the container starts
CMD ["bash"]
