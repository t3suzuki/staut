FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

ARG USERNAME=tomoya-s

RUN apt -y update
RUN apt install -y git sudo cmake g++ screen emacs

RUN useradd -m $USERNAME
RUN gpasswd -a $USERNAME sudo
RUN echo "${USERNAME}:hogehoge" | chpasswd

USER $USERNAME
RUN git config --global user.email "t3suzuki@ucsd.edu"
RUN git config --global user.name "Tomoya Suzuki"

WORKDIR /home/$USERNAME/
RUN git clone https://github.com/t3suzuki/my_configs
RUN cp my_configs/.screenrc ./

WORKDIR /home/$USERNAME/
RUN git clone --recursive https://github.com/t3suzuki/staut

RUN apt install -y autoconf libtool zlib1g-dev liburing-dev binutils-dev libgflags-dev
