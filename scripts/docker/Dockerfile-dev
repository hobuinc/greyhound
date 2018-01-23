FROM connormanning/greyhound
MAINTAINER Connor Manning <connor@hobu.co>

RUN \
    echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> \
        /etc/apk/repositories; \
    apk update; \
    apk add --no-cache \
        gdb \
        ;

ENTRYPOINT ["sh"]

# Sample invocation:
#       docker run -it --name gdb \
#           --security-opt seccomp=unconfined --cap-add=SYS_PTRACE \
#           -p 8080:8080 \
#           -v ~/greyhound:/opt/data \
#           connormanning/greyhound \
#           -c "(nohup greyhound -c /opt/home/config.json > /var/log/greyhound.txt &) && tail -f /var/log/greyhound.txt"
#
# As one line:
#       docker run -it --name gdb --security-opt seccomp=unconfined --cap-add=SYS_PTRACE -p 8080:8080 -v ~/greyhound:/opt/data connormanning/greyhound -c "(nohup greyhound -c /opt/home/config.json > /var/log/greyhound.txt &) && tail -f /var/log/greyhound.txt"


# To attach once running:
#       docker exec -it gb sh
#       top     # Find `greyhound` PID
#       gdb -p <PID>

