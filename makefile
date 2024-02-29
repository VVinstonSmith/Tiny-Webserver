CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./http/util.cpp ./http/write_html.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  ./webserver/webserver.cpp config.cpp
	$(CXX) -std=c++11 -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server
