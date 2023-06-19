
DIR = $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
NAME = $(shell basename $(shell dirname $(DIR)))
REGISTRY = some-registry:5000
#TAG = docs-$(NAME)
TAG = docs-$(shell echo $(NAME) | tr [:upper:] [:lower:])

all: build push
cleanbuild: clean build

clean:
	#docker image prune -a -f && docker volume prune -f
	#docker image prune -f -a && docker volume prune -f
	#docker system prune -f

build:
	# building docker image $(TAG) ...
	cd $(DIR)/.. && docker build . -f documentation/Dockerfile -t $(TAG)

run:
	docker run --rm -it -p 3000:3000 $(TAG)

push:
	#docker image push $(REGISTRY)/$(TAG)"
