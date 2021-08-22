FROM ubuntu:21.10

ENV DEBIAN_FRONTEND noninteractive

RUN  dpkg --add-architecture i386 &&  apt-get update 
## apt-get upgrade -y 
RUN apt-get install -y libboost-all-dev libjsoncpp-dev openjdk-11-jdk libc6:i386 libstdc++6:i386 zipalign apksigner python3.10

## Installing git-lfs 
RUN apt install -y curl

ENV os ubuntu
ENV dist trustly 

RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh >> ./script.sh && bash -c os=ubuntu dist=trusty ./script.sh &&  apt-get install git-lfs -y 
RUN git clone https://github.com/Gyoonus/deoptfuscator.git
