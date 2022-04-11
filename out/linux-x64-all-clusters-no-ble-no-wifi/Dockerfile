FROM ubuntu:20.04

ARG USERNAME=chip

ARG DEBIAN_FRONTEND=noninteractive

# Install necessary software
RUN apt-get update && apt-get install --no-install-recommends -y \
    ca-certificates \
    gcc \
    libc6-dev \
    libffi-dev \
    libglib2.0-dev \
    libmount-dev \
    libpcre3-dev \
    libssl-dev \
    locales \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Setup locales
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
    && locale-gen

ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en

# Create a new user
RUN useradd -ms /bin/bash $USERNAME

COPY chip-all-clusters-app /chip-all-clusters-app

USER $USERNAME

EXPOSE 5540/udp

EXPOSE 5353/udp

ENTRYPOINT [ "/chip-all-clusters-app" ]