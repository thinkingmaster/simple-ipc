/*
 * main.cpp
 *
 *  Created on: Feb 5, 2017
 *      Author: nullifiedcat
 */

#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <time.h>
#include <csignal>

#include "include/cmp.hpp"
#include "include/iipc.hpp"

static bool is_server = false;

#define CMD_TYPE_MESSAGE 1
#define CMD_TYPE_MESSAGE_LONG 2

struct server_data { int one; };
struct client_data { bool test; };

#define ESC_PRINTF(s, ...) printf("\033[" s, __VA_ARGS__)
#define ESC_CUP(x, y) ESC_PRINTF("%d;%dH", (y), (x))
#define ESC_EL(n) ESC_PRINTF("%dK", (n))
#define ESC_SGR(n) ESC_PRINTF("%dm", (n))
#define ESC_ED(n) ESC_PRINTF("%dJ", (n));

#define TEXT_BOLD ESC_SGR(1)
#define TEXT_NORMAL ESC_SGR(0)

std::string server_name = "cat_ipc_test";

cat_ipc::server& server()
{
    static cat_ipc::server<server_data, client_data> object(server_name);
    return object;
}

cat_ipc::client& client()
{
    static cat_ipc::client<server_data, client_data> object(server_name);
    return object;
}

void print_status() {
	ESC_CUP(1, 1);
	ESC_ED(2);
	fflush(stdout);
	ESC_CUP(2, 2);
	TEXT_BOLD;
	printf("ipc server %s", server_name.c_str());
	ESC_CUP(3, 4);
	printf("connected: ");
	TEXT_NORMAL; printf("%u", server().memory->client_count); TEXT_BOLD;
	ESC_CUP(3, 5);
	printf("command count: ");
	TEXT_NORMAL; printf("%u", server().memory->command_count); TEXT_BOLD;
	ESC_CUP(3, 6);
	printf("command memory pool stats: ");
	CatMemoryPool::pool_info_s info;
	peer().pool->statistics(info);
	ESC_CUP(4, 8); ESC_EL(2);  printf("total:     ");
	ESC_CUP(4, 9); ESC_EL(2);  printf("free:      ");
	ESC_CUP(4, 10); ESC_EL(2); printf("allocated: ");
	TEXT_NORMAL;
	ESC_CUP(16, 8);  printf("%lu (%u blocks)", info.free + info.alloc, info.blkcnt);
	ESC_CUP(16, 9); printf("%lu (%u blocks)", info.free, info.freeblk);
	ESC_CUP(16, 10); printf("%lu (%u blocks)", info.alloc, info.allocblk);
	ESC_CUP(3, 12);
	TEXT_BOLD;
	printf("client list: ");
	TEXT_NORMAL;
	for (unsigned i = 0; i < cat_ipc::max_clients; i++) {
		if (!server().memory->clients[i].free) {
			printf("%u (%d) ", i, server().memory->clients[i].process);
		}
	}
	ESC_CUP(1, 14);
	fflush(stdout);
}

void* listen_for_messages(void* argument) {
	while (true) {
		client().ProcessCommands();
		usleep(1000);
	}
	return 0;
}

/*
 * This requires a little bit of explanation.
 * server (manager) 's job is to allocate/free shared memory segment and collect dead clients.
 * everything else is done by peers
 */

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s [server|client]\n", argv[0]);
		return 0;
	}
	if (argc > 2) {
		server_name = std::string(argv[2]);
	}
	if (!strcmp(argv[1], "server")) {
		is_server = true;
		server();
		while (true) {
			server().update();
			print_status();
			usleep(10000);
		}
	} else if (!strcmp(argv[1], "client")) {
		client().connect(false);
		peer().SetCommandHandler(CMD_TYPE_MESSAGE, [](cat_ipc::command_data& command, void* payload) {
			printf("%u says: %s\n", command.sender, command.cmd_data);
		});
		peer().SetCommandHandler(CMD_TYPE_MESSAGE_LONG, [](cat_ipc::command_data& command, void* payload) {
			printf("%u says: %s\n", command.sender, (char*)payload);
		});
		pthread_t thread;
		pthread_create(&thread, 0, listen_for_messages, 0);
		char* buffer = new char[1024 * 1024 * 1024];
		while (true) {
			fgets(buffer, 1024 * 1024 * 1024, stdin);
			buffer[strlen(buffer) - 1] = '\0';
			if (strlen(buffer) > 63) {
				client().send_message(0, 0, CMD_TYPE_MESSAGE_LONG, buffer, strlen(buffer) + 1);
			} else {
				client().send_message(buffer, 0, CMD_TYPE_MESSAGE, 0, 0);
			}
		}
	}

}
