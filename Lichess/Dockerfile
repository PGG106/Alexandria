FROM ghcr.io/pgg106/alexandria:2.2 as LAUNCH

RUN apt-get update &&\
    apt-get install -y python3 python3-pip git 

RUN git clone https://github.com/ShailChoksi/lichess-bot.git /lcbot

RUN mv /src/Alexandria /lcbot/engines/Alexandria

WORKDIR "/lcbot"
COPY ["config.yml", "config.yml"]
RUN pip3 install wheel &&\
    pip3 install -r requirements.txt
CMD ["/lcbot/lichess-bot.py", "-v"]
ENTRYPOINT ["python3"]
