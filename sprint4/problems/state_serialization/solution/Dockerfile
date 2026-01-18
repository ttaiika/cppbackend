# Не просто создаём образ, но даём ему имя build 
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.59.0

# Запуск conan
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Копируем ВСЕ исходные файлы
COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

# Сборка проекта с тестами
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON .. && \
    cmake --build . --target game_server

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static       
EXPOSE 8080

ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static/"]