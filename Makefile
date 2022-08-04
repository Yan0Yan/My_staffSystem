all:server client

server:
	gcc ./app_server/*.c -lsqlite3 -pthread -o ./run_server

client:
	gcc ./app_client/*.c -o ./run_client

clean:
	rm ./run_*
