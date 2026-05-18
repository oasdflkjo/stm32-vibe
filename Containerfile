FROM fedora:44

RUN dnf -y install \
    arm-none-eabi-gcc-cs \
    arm-none-eabi-newlib \
    git \
    make \
    openocd \
    stlink \
  && dnf clean all

WORKDIR /workspace

CMD ["make"]
