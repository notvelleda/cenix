FROM archlinux
WORKDIR /
RUN pacman --noconfirm -Syu
RUN pacman --noconfirm -S zig make gcc base-devel
RUN mkdir -p /home/user
RUN groupadd -g 1000 user
RUN useradd -u 1000 -g 1000 -s /bin/sh -d /home/user user
RUN chown user:user /home/user
RUN chmod 777 /home/user
