## ETAPA 1: COMPILACIÓN (BUILD)
FROM alpine:latest AS build
RUN apk add --no-cache build-base cmake git linux-headers openssl-dev openssl-libs-static zlib-dev zlib-static sqlite-dev sqlite-static libuv-dev libuv-static jansson-dev ca-certificates
RUN echo "appuser:x:10001:10001:appuser:/:/sbin/nologin" > /etc/passwd_app
WORKDIR /app
COPY ./backend/ . 
RUN make

## ETAPA 2: IMAGEN FINAL
FROM alpine AS runtime
# Importar usuario
COPY --from=build /etc/passwd_app /etc/passwd

# Crear directorio para certs y asignar dueño ANTES de cambiar a USER appuser
RUN mkdir -p /etc/ssl/app && chown -R 10001:10001 /etc/ssl/app

# Copiar el binario
COPY --from=build /app/app /bin/app

# Copiar certificados y asignar propiedad al usuario 10001
COPY --chown=10001:10001 ./backend/certs/ /etc/ssl/app/

EXPOSE 80

# Cambiar al usuario sin privilegios
USER appuser
# Definir el directorio de trabajo
WORKDIR /bin
ENTRYPOINT ["./app"]