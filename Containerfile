FROM fedora:44

RUN dnf -y install \
    arm-none-eabi-gcc-cs \
    arm-none-eabi-newlib \
    gcc \
    git \
    make \
    openocd \
    python3 \
    stlink \
  && dnf clean all

WORKDIR /workspace

CMD ["make"]
