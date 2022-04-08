FROM ubuntu:20.04

ARG USERNAME=vscode

ARG DEBIAN_FRONTEND=noninteractive

# Install necessary software
RUN apt-get update && apt-get install --no-install-recommends -y \
    bison \
    build-essential \
    ca-certificates \
    ccache \
    clang-format \
    cmake \
    curl \
    dfu-util \
    flex \
    gcc-multilib \
    git \
    gnupg \
    gperf \
    libavahi-client-dev \
    libcairo2-dev \
    libdbus-1-dev \
    libffi-dev \
    libgirepository1.0-dev \
    libglib2.0-dev \
    libncurses5 \
    libreadline-dev \
    libssl-dev \
    libusb-1.0-0 \
    locales \
    nano \
    ninja-build \
    pkg-config \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-venv \
    sudo \
    unzip \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Setup locales
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
    && locale-gen

ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en

# Create a new sudo user with the right permissions
RUN groupadd -g 986 ttyusb && \
    useradd -ms /bin/bash -G dialout,plugdev,ttyusb $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME

USER $USERNAME

WORKDIR /home/$USERNAME

# Install esp-idf
RUN mkdir tools \
    && cd tools \
    && git clone -b v4.4 --recurse-submodules -j8 https://github.com/espressif/esp-idf.git \
    && cd esp-idf \
    && ./install.sh

# Setup terminal
RUN echo '. $HOME/tools/esp-idf/export.sh' >> ~/.bashrc && \
    echo 'source ./scripts/activate.sh' >> ~/.bashrc
