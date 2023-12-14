
#FROM	docs-docker-base
FROM	gcr.io/pixo-bootstrap/docs-docker-base:latest

ARG	CACHEBREAKER=https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h

WORKDIR	/build

COPY	. .

RUN	doxygen -v	# confirm doxygen version from docs-docker-base

ADD	$CACHEBREAKER skipcache

# ensure the doxygen stuff is most up to date
RUN	cd /build && \
	git submodule update --init --recursive && \
	cd documentation/docs-doxygen && git pull origin main

RUN	git fetch --tags

#ADD	$CACHEBREAKER skipcache

RUN	./documentation/build.sh && \
	eval "$(cat ./documentation/env.sh | grep "PROJECT_URL")" && \
	mkdir -p /var/www/html/$PROJECT_URL && \
	cp -r ./documentation/html/* /var/www/html/$PROJECT_URL

#ENTRYPOINT ["/docker-entrypoint.sh"]
#CMD	["nginx", "-g", "daemon off;"]



