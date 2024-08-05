FROM ubuntu:22.04

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

USER root
RUN apt install -y autoconf libtool zlib1g-dev liburing-dev binutils-dev libgflags-dev
USER $USERNAME

WORKDIR /home/$USERNAME/staut/rocksdb
RUN git checkout v7.2.2
RUN mkdir build
WORKDIR /home/$USERNAME/staut/rocksdb/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release
RUN make -j 8

WORKDIR /home/$USERNAME/staut/wiredtiger
RUN git checkout v11.2.0
RUN mkdir build
WORKDIR /home/$USERNAME/staut/wiredtiger/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release
RUN make -j 8


