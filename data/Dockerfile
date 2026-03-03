# ETAPA 1: Compilación
FROM alpine:3.19 AS builder

RUN apk add --no-cache build-base bison flex curl linux-headers util-linux-dev readline-dev zlib-dev

ENV PG_VERSION=16.2
RUN curl -L https://ftp.postgresql.org/pub/source/v$PG_VERSION/postgresql-$PG_VERSION.tar.gz | tar xz

WORKDIR /postgresql-$PG_VERSION

RUN ./configure --prefix=/opt/pgsql --without-icu --without-readline --without-zlib --disable-rpath
RUN make -j$(nproc) && make install

# --- PREPARACIÓN DEL ROOTFS ---
RUN mkdir -p /rootfs/bin /rootfs/lib /rootfs/etc/postgresql /rootfs/data /rootfs/tmp /rootfs/share

# 1. Copiamos binarios
RUN cp /opt/pgsql/bin/postgres /opt/pgsql/bin/initdb /rootfs/bin/

# 2. Copiamos librerías y extensiones (AQUÍ ESTABA EL ERROR)
# Copiamos las lib del sistema
RUN ldd /opt/pgsql/bin/postgres | grep "=> /" | awk '{print $3}' | xargs -I '{}' cp -v '{}' /rootfs/lib/
RUN cp /lib/ld-musl-x86_64.so.1 /rootfs/lib/
# Copiamos las lib propias de postgres y sus plugins (.so) a /rootfs/lib
RUN cp -P /opt/pgsql/lib/*.so* /rootfs/lib/

# 3. Copiamos los archivos share
RUN cp -r /opt/pgsql/share/* /rootfs/share/

# 4. CREACIÓN DE USUARIO
RUN addgroup -g 70 -S postgres && adduser -u 70 -S -G postgres postgres
RUN echo "postgres:x:70:70:postgres:/data:/bin/false" > /rootfs/etc/passwd && \
    echo "postgres:x:70:" > /rootfs/etc/group

# 5. PERMISOS
RUN chown -R 70:70 /rootfs/data /rootfs/etc/postgresql /rootfs/tmp /rootfs/share /rootfs/lib

# 6. PRE-INICIALIZACIÓN
USER postgres
# Seteamos LD_LIBRARY_PATH para que encuentre dict_snowball.so en /rootfs/lib
# Usamos --no-instructions para evitar que intente buscar herramientas de sistema que no hay en scratch
RUN LD_LIBRARY_PATH=/rootfs/lib /rootfs/bin/initdb \
    -L /rootfs/share \
    -D /rootfs/data \
    --locale=C --encoding=UTF8

# 7. CONFIGURACIÓN FINAL
USER root
RUN echo "listen_addresses = '*'" > /rootfs/etc/postgresql/postgresql.conf && \
    echo "port = 5432" >> /rootfs/etc/postgresql/postgresql.conf && \
    echo "unix_socket_directories = '/tmp'" >> /rootfs/etc/postgresql/postgresql.conf && \
    chown 70:70 /rootfs/etc/postgresql/postgresql.conf

# ETAPA 2: Scratch
FROM scratch
COPY --from=builder /rootfs /

ENV PATH="/bin:$PATH"
ENV PGDATA=/data
# Esto es vital para que Postgres encuentre sus plugins en la imagen final
ENV LD_LIBRARY_PATH="/lib"

USER postgres
WORKDIR /data
EXPOSE 5432

CMD ["postgres", "-D", "/data", "-c", "config_file=/etc/postgresql/postgresql.conf"]