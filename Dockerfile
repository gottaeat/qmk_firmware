FROM debian:12.11 AS qmk

COPY . /repo
WORKDIR /repo

RUN \
    apt update && apt upgrade -y && apt install -y --no-install-recommends \
        build-essential sudo git vim python3-pip && \
    pip3 install qmk --break-system-packages && \
    qmk doctor -y || true
