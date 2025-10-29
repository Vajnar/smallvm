FROM ubuntu:24.04

ARG GID
ARG UID

# disable interactive functions
ENV DEBIAN_FRONTEND noninteractive

RUN dpkg --add-architecture i386
RUN apt-get update && \
    # install needed packages
    apt-get install -y apt-utils && \
    apt-get install -y --no-install-recommends \
      ca-certificates language-pack-en language-pack-en-base \
      less vim curl ssh \
      gawk wget git-core diffstat zip unzip texinfo gcc-multilib build-essential \
      chrpath socat repo libsdl1.2-dev xterm cpio file bison \
      screen ncurses-dev sudo \
      libsdl2-dev:i386 libsdl2-ttf-dev:i386 libpng-dev:i386 libfreetype-dev \
      tzdata && \
    # upgrade OS
    apt-get -y dist-upgrade && \
    apt-get autoremove -y && \
    apt-get clean all

RUN mkdir -p /home/builder/.ssh/
RUN /bin/echo -ne "Host *\n    StrictHostKeyChecking no\n    UserKnownHostsFile=/dev/null\n" >> /home/builder/.ssh/config

RUN locale-gen en_US.UTF-8
ENV LANG='en_US.UTF-8' LANGUAGE='en_US:en' LC_ALL='en_US.UTF-8' TERM=screen

ENV UID $UID
ENV GID $GID

RUN echo "GID ... $GID"
RUN echo "UID ... $UID"
RUN deluser --remove-home ubuntu
RUN addgroup --gid $GID builder
RUN adduser --gid $GID --uid ${UID} --home /home/builder --no-create-home --shell /bin/bash --disabled-password --gecos "" builder
RUN id -u builder
RUN echo "builder      ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers
RUN chown -R builder:builder /home/builder

USER builder
WORKDIR /workdir
