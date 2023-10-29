FROM ubuntu:22.04

RUN apt-get update && apt-get install -y build-essential libsqlite3-dev libssl-dev libbsd-dev
RUN DEBIAN_FRONTEND='noninteractive' apt-get install -y valgrind vim git flex bison

CMD ["bash"]
