/*
 * main.c
 *
 *  Created on: May 21, 2014
 *      Author: indranil
 */


// NOTE :
//        >> TO FIX - NUMBER OF HOPS
//        >> TO IMPLEMENT - 3 TIMEOUTS, DV NOT RECEIVED FROM THE DISABLED SERVER...MAKE INFINITY


// HEADER FILES

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/fcntl.h>

char portaddr[20];

// DEFINING A USER-DEFINED STRUCTURE

struct calc_table
{
	char hop_cost[10][7];
	char next_router[10][2];
	char no_of_hops[10][2];
};

// A FUNCTION FOR THIS SERVER'S IP ADDRESS

char *myip()
{
	int temp_sockfd, check;
	char buffer[100];
	struct sockaddr_in serv;

	temp_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (temp_sockfd < 0)
	{
		perror("Error in socket()");
	}

	memset(&serv, 0, sizeof(serv));
	memset(buffer, 0, sizeof(buffer));

	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr("8.8.8.8");
	serv.sin_port = htons(53);

	check = connect(temp_sockfd, (const struct sockaddr*)&serv, sizeof(serv));
	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);
	check = getsockname(temp_sockfd, (struct sockaddr*)&name, &namelen);
	const char *ip = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);

	if (ip != NULL && check != -1)
	{
		strcpy(portaddr, buffer);
	}
	else
	{
		printf("Error %d %s \n", errno, strerror(errno));
	}
	close(temp_sockfd);
	return portaddr;
}

// MAIN FUNCTION STARTS

int main()
{

	//INTEGER DECLARATIONS

	int no_of_servers;
	int no_of_neighbours;
	int max_cmd;
	int max_cmd_2;
	int sock_select, max_sock;
	int i = 0, j = 0, k = 0, l = 0, m = 0;
	int len_cmd;
	int routing_update_interval;
	int cmd_buf = 100;
	int this_server_port;
	int l_sock;
	int errcheck;
	int this_server;
	int server_id[20];
	int server_port[20];
	int neighbour_id[20];
	int no_of_packets = 0;
	int packet_sender = 0;
	int crash_status = 0;
	int exit_server = 0;
	int intermediate_update_interval;
	int sender_server_id;
	int dv_algorithm = 0;
	int least_cost_a_b;
	int step_send_packet_now = 0;
	int mismatched_topology_file = 0;
	int hop_value[10];
	int dv_copy_all=0;
	int next_router_check = 0;

	// CHARACTER DECLARATIONS

	char c;

	// STRING DECLARATIONS

	char store[50][100];
	char cmd[5][100];
	char *command;
	char path[100];
	char check[10];
	char buffer[1024];
	char server_ip[20][100];
	char cost_to_server[20][10];
	char cost[20][10];
	char neighbour_status[20][2];
	char incoming_status[2];
	char message[1024];

	char m_hop[2];
	char m_update_fields[2];
	char m_server_port[10];
	char m_server_ip[20];
	char m_server_id[2];

	// STRUCTURE DECLARATIONS

	struct sockaddr_in server;
	struct sockaddr_in n_server;
	struct timeval interval, select_start, select_return;
	struct calc_table calculation[5];

	socklen_t addrlen = sizeof(n_server);

	// FILE POINTER DECLARATION

	FILE *topologyFile;

	// FILE DESCRIPTOR SET DECLARATION

	fd_set masterfdset, clonefdset;

	// ALLOCATING MEMORY TO COMBAT GARBAGE VALUES

	for (l=0;l<5;l++)
	{
		for (m=0;m<5;m++)
		{
			strcpy(calculation[i].hop_cost[j],"");
			strcpy(calculation[i].next_router[j],"");
			strcpy(calculation[i].no_of_hops[j],"");
		}
	}

	//COMMAND INPUT

	while (1)
	{
		// FILE PATH

		strcpy(path, "//home//indranil//mnc//proj2//");

		// COMMAND INPUT

		printf("proj2>>");

		command = (char *) malloc(cmd_buf + 1);
		getline(&command, (size_t *) &cmd_buf, stdin);

		// GET ARGUMENTS FROM THE COMMAND

		len_cmd = strlen(command);
		for (i = 0; i < len_cmd; i++)
		{
			c = command[i];
			if (c != ' ' && c != '\n' && c != '\0')
			{
				cmd[j][k] = c;
				k++;
			}
			else
			{
				cmd[j][k] = '\0';
				j++;
				k = 0;
			}
		}
		max_cmd = j;
		j = 0;
		k = 0;
		i = 0;

		// ERROR HANDLING AND READING FILE

		if (strcasecmp(cmd[0], "SERVER") == 0 && strcasecmp(cmd[1], "-t") == 0 && strcasecmp(cmd[3], "-i") == 0)
		{
			sprintf(check,"%d",atoi(cmd[4]));
			if (strcmp(check,cmd[4]) != 0)
			{
				printf("Routing update interval has to be numerical...please re-enter\n");
				continue;
			}

			// READING FILE AND INITIALIZING VARIABLES

			routing_update_interval = atoi(cmd[4]);
			strcat(path, cmd[2]);
			topologyFile = fopen(path, "r");

			if (topologyFile != NULL)
			{
				while ((c = fgetc(topologyFile)) != EOF)
				{
					if (c != '\n' && c != ' ')
					{
						store[i][j] = c;
						j++;
					}
					else
					{
						store[i][j] = '\0';
						i++;
						j = 0;
					}
				}
				fclose(topologyFile);
			}
			else
			{
				printf("file not found\n");
				continue;
			}

			no_of_servers = atoi(store[0]);
			no_of_neighbours = atoi(store[1]);

			for (i = 0; i < no_of_servers; i++)
			{
				server_id[i] = atoi(store[2 + (i * 3)]);
				strcpy(server_ip[i], store[3 + (i * 3)]);
				server_port[i] = atoi(store[4 + (i * 3)]);
			}

			for (i = 0; i < no_of_neighbours; i++)
			{
				this_server = atoi(store[(no_of_servers * 3) + 2 + (i * 3)]);
				neighbour_id[i] = atoi(store[(no_of_servers * 3) + 3 + (i * 3)]);
				strcpy(cost[i],store[(no_of_servers * 3) + 4 + (i * 3)]);
				strcpy(neighbour_status[i],"E");
			}

			for (i=0;i<no_of_servers;i++)
			{
				if (server_id[i] == this_server)
				{
					if (strcmp(server_ip[i],myip()) == 0)
					{
						this_server_port = server_port[i];
						break;
					}
					else
					{
						printf("Fatal Error, A Mismatch in the topology file...\n");
						printf("Please enter the correct topology file for this Router...\n");
						mismatched_topology_file = 1;
					}
				}
			}

			if (mismatched_topology_file == 1)
			{
				mismatched_topology_file = 0;
				continue;
			}

			// PRINTING OUT THE DATA COLLECTED FROM THE FILE

			printf("SERVER TOPOLOGY FILE DATA: \n");
			printf("==============================================\n");
			printf("Number of servers : %d \n", no_of_servers);
			printf("Number of servers in the neighborhood : %d \n", no_of_neighbours);
			printf("Routing update interval specified : %d sec \n", routing_update_interval);

			for (i = 0; i < no_of_servers; i++)
			{
				for (j = 0; j < no_of_neighbours; j++)
				{
					if (server_id[i] == neighbour_id[j])
					{
						strcpy(cost_to_server[i], cost[j]);
						hop_value[i] = 1;
						break;
					}
					else if (server_id[i] == this_server)
					{
						strcpy(cost_to_server[i], "0");
						hop_value[i] = 0;
						break;
					}
					strcpy(cost_to_server[i], "inf");
					hop_value[i] = 0;
				}
			}

			printf("==============================================\n");
			printf("ID\tServer IP\tPort\tCostToServer\n");
			printf("----------------------------------------------\n");
			for (i = 0; i < no_of_servers; i++)
			{
				printf("%d\t%s\t%d\t%s\n", server_id[i], server_ip[i], server_port[i], cost_to_server[i]);
			}
			printf("==============================================\n");
			printf("ServerID\tNeighbourID\tCost\n");
			printf("----------------------------------------------\n");
			for (i = 0; i < no_of_neighbours; i++)
			{
				printf("%d\t\t%d\t\t%s\n", this_server, neighbour_id[i], cost[i]);
			}
			printf("==============================================\n");
			break;
		}

		// ERROR HANDLING ON THE SERVER COMMAND

		else
		{
			printf("Command syntax error...\n");
			printf("Syntax : \n server -t <topology-file-name> -i <routing-update-interval>\n");
		}
	}

	// OPENING SOCKET ON SERVER

	l_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (l_sock < 0)
	{
		perror("ERROR IN OPENING SOCKET...");
		exit(-1);
	}

	errcheck = fcntl(l_sock, F_SETFL, O_NONBLOCK);
	if (errcheck < 0)
	{
		perror("ERROR IN NONBLOCKING");
		close(l_sock);
		exit(-1);
	}

	// STORING THIS SERVER'S IP ADDRESS AND LISTENING PORT AS EXTRACTED FROM TOPOLOGY FILE

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(myip());
	server.sin_port = htons(this_server_port);

	// BINDING THE SOCKET

	errcheck = bind(l_sock, (struct sockaddr *)&server, sizeof(server));
	if (errcheck < 0)
	{
		perror("ERROR IN BINDING");
		close(l_sock);
		exit(-1);
	}

	printf("SERVER IS ON \n");

	// ADDING STDIN AND LISTENING/SENDING SOCKET TO THE FILE DESCRIPTOR SET

	FD_ZERO(&masterfdset);
	FD_ZERO(&clonefdset);
	max_sock = l_sock;
	FD_SET(l_sock,&masterfdset);
	FD_SET(0,&masterfdset);
	intermediate_update_interval = routing_update_interval;

	// THE SERVER STARTS I/O AND WAITS FOR DATA FROM ANOTHER SERVER

	while(exit_server == 0)
	{
		memcpy(&clonefdset, &masterfdset, sizeof(masterfdset));
		sock_select = max_sock + 1;

		// SETTING THE SERVER TIMEOUT TO SEND ROUTING UPDATE

		intermediate_update_interval = intermediate_update_interval - (select_return.tv_sec - select_start.tv_sec);
		interval.tv_sec = intermediate_update_interval;
		interval.tv_usec = 0;

		if (intermediate_update_interval <= 0)
		{
			interval.tv_sec = routing_update_interval;
			interval.tv_usec = 0;
			intermediate_update_interval = routing_update_interval;
		}

		if (step_send_packet_now == 1)
		{
			interval.tv_sec = 0;
			interval.tv_usec = 0;
		}
		else
		{
			printf("server >> ");
		}
		fflush(stdout);

		gettimeofday(&select_start, NULL);

		// USING SELECT() FOR ASYNCHRONOUS I/O

		if (crash_status != 1)
		{
			errcheck = select(sock_select, &clonefdset, NULL, NULL, &interval);
		}
		else
		{
			errcheck = select(sock_select, &clonefdset, NULL, NULL, NULL);
		}

		if (errcheck < 0)
		{
			perror("error in select()");
			break;
		}

		step_send_packet_now = 0;
		packet_sender = 0;

		// STDIN IN THE SERVER SOCKET - SOCK

		if (FD_ISSET(0, &clonefdset))
		{
			packet_sender = 1;

			memset(&buffer, 0, sizeof(buffer));
			read(0,buffer,sizeof(buffer));

			len_cmd = strlen(buffer);
			j = 0;
			k = 0;
			for (i = 0; i < len_cmd; i++)
			{
				c = buffer[i];
				if (c != ' ' && c != '\n' && c != '\0')
				{
					cmd[j][k] = c;
					k++;
				}
				else
				{
					cmd[j][k] = '\0';
					j++;
					k = 0;
				}
			}
			max_cmd = j;
			j = 0;
			k = 0;

			// UPDATE THE COST TABLE

			if (strcasecmp(cmd[0],"UPDATE") == 0)
			{

				// CHECKING FOR ERROR IN THE UPDATE COMMAND

				for (i=0; i<no_of_neighbours && (max_cmd == 4) && (atoi(cmd[1]) == this_server);i++)
				{
					if (atoi(cmd[2]) == neighbour_id[i])
					{
						strcpy(cost[i],cmd[3]);
						errcheck = 10;
					}
				}

				// IF NO ERROR THEN UPDATE AND SEND TO THE OTHER SERVER

				if (errcheck == 10)
				{
					for (i = 0; i < no_of_servers; i++)
					{
						for (j = 0; j < no_of_neighbours; j++)
						{
							if (server_id[i] == neighbour_id[j])
							{
								strcpy(cost_to_server[i], cost[j]);
								break;
							}
						}
					}

					printf("UPDATE SUCCESSFUL\n");

					// PUT ADDR OF OTHER SERVER

					memset(&n_server, 0, sizeof(n_server));
					for (i=0;i<no_of_servers;i++)
					{
						if (atoi(cmd[2]) == server_id[i])
						{
							n_server.sin_family = AF_INET;
							n_server.sin_addr.s_addr = inet_addr(server_ip[i]);
							n_server.sin_port = htons(server_port[i]);
						}
					}

					// SEND TO OTHER SERVER

					sendto(l_sock, buffer, strlen(buffer), 0, (struct sockaddr *)&n_server, sizeof(n_server));

					gettimeofday(&select_return, NULL);
					continue;
				}

				// ERROR HANDLING ON UPDATE IF THE ENTERED NEIGHBOUR-ID IS WRONG

				else
				{
					printf("UPDATE FAILED\n");

					gettimeofday(&select_return, NULL);
					continue;
				}
			}

			// DISPLAY CALCULATION TABLE

			else if ((strcasecmp(cmd[0],"CALC") == 0) && (max_cmd == 1))
			{
				printf("CALC TABLE\n");
				for (i=0;i<no_of_servers;i++)
				{
					for (j=0;j<no_of_servers;j++)
					{
						if (atoi(calculation[i].hop_cost[j]) < 20)
						{
							printf("%s\t", calculation[i].hop_cost[j]);
						}
						else
						{
							printf("inf\t");
						}
					}
					printf("\n");
				}
				gettimeofday(&select_return, NULL);
				continue;
			}

			// CRASH THE SERVER

			else if ((strcasecmp(cmd[0],"CRASH") == 0) && (max_cmd == 1))
			{
				crash_status = 1;
				printf("Crash induced on server...\n");
				gettimeofday(&select_return, NULL);
				continue;
			}


			// RECOVER FROM CRASH

			else if ((strcasecmp(cmd[0],"RECOVER") == 0) && (max_cmd == 1))
			{
				crash_status = 0;
				printf("Recovered from the induced Crash...\n");
				gettimeofday(&select_return, NULL);
				continue;
			}

			// DISPLAY THE ROUTING TABLE FOR THIS SERVER

			else if ((strcasecmp(cmd[0],"DISPLAY") == 0) && (max_cmd == 1))
			{
				if (crash_status == 1)
				{
					printf("Cant Display...Server is in crash mode...\n");
					gettimeofday(&select_return, NULL);
					continue;
				}
				printf("                  ROUTING TABLE OF THIS SERVER\n");
				printf("===================================================================\n");
				printf("Destination\tNext Router\tNo of Hops to Destination\tCost\n");
				printf("-------------------------------------------------------------------\n");
				for (i = 0; i < no_of_servers; i++)
				{
					if (server_id[i] == this_server)
					{
						for (j=0;j<no_of_servers;j++)
						{
							if (strlen(calculation[i].next_router[j]) != 0)
							{
								printf("%d\t\t%s\t\t%d\t\t\t\t%s\n",server_id[j],calculation[i].next_router[j],hop_value[j],calculation[i].hop_cost[j]);
							}
							else
							{
								printf("%d\t\t%d\t\t%d\t\t\t\t%s\n",server_id[j],this_server,hop_value[j],calculation[i].hop_cost[j]);
							}
						}
					}
				}
				printf("===================================================================\n");
				gettimeofday(&select_return, NULL);
				continue;
			}

			// DISABLE ANOTHER SERVER

			else if ((strcasecmp(cmd[0],"DISABLE") == 0) && (max_cmd == 2))
			{

				// IF ENTERED ID IS A NEIGHBOUR : PROCESS

				for (i=0;i<no_of_neighbours;i++)
				{
					if (atoi(cmd[1]) == neighbour_id[i])
					{
						strcpy(neighbour_status[i],"D");
						errcheck = 20;
					}
				}

				// ERROR HANDLING IN DISABLE

				if (errcheck == 20)
				{
					printf("Disabled %s\n",cmd[1]);
				}
				else if (atoi(cmd[1]) == this_server)
				{
					printf("Cannot disable your own server...\n");
				}
				else
				{
					printf("ID entered is not a neighbour...\n");
				}
				gettimeofday(&select_return, NULL);
				continue;
			}

			// STEP : SEND PACKET IMMEDIATELY

			else if ((strcasecmp(cmd[0],"STEP") == 0) && (max_cmd == 1))
			{
				if (crash_status != 1)
				{
					step_send_packet_now = 1;
					printf("Forced broadcast of routing update...\n");
					gettimeofday(&select_return, NULL);
					continue;
				}
				else
				{
					printf("Server is in crash mode...Cant send PACKET\n");
					gettimeofday(&select_return, NULL);
					continue;
				}
			}

			// PACKETS : NUMBER OF PACKETS RECEIVED

			else if ((strcasecmp(cmd[0],"PACKETS") == 0) && (max_cmd == 1))
			{
				printf("Packets Received : %d\n", no_of_packets);
				no_of_packets = 0;
				gettimeofday(&select_return, NULL);
				continue;
			}

			// EXIT : TO EXIT THE PROGRAM

			else if ((strcasecmp(cmd[0],"EXIT") == 0) && (max_cmd == 1))
			{
				printf("Quitting program and stopping server...\n");
				exit_server = 1;
				continue;
			}

			// ERROR HANDLING ON STDIN

			else
			{
				printf("WRONG COMMAND : CHECK SYNTAX\n");
				gettimeofday(&select_return, NULL);
				continue;
			}
		}

		//  I/O FROM ANOTHER CONNECTION

		for (i=max_sock;i <= max_sock;i++)
		{
			if (FD_ISSET(i, &clonefdset))
			{
				packet_sender = 1;
				memset(&buffer, 0, sizeof(buffer));

				// RECEIVE DATA FROM ANOTHER SERVER ON THE LISTENING UDP SOCKET

				recvfrom(i, buffer, 1024, 0, (struct sockaddr *)&n_server, &addrlen);

				// PROCESS THE DATA RECEIVED

				len_cmd = strlen(buffer);
				j = 0;
				k = 0;
				for (l = 0; l < len_cmd; l++)
				{
					c = buffer[l];
					if (c != ' ' && c != '\n' && c != '\0')
					{
						cmd[j][k] = c;
						k++;
					}
					else
					{
						cmd[j][k] = '\0';
						j++;
						k = 0;
					}
				}
				max_cmd_2 = j;
				j = 0;
				k = 0;

				// CHECK STATUS OF SENDING NEIGHBOUR

				for (l=0;l<no_of_servers;l++)
				{
					if ((strcasecmp(server_ip[l],cmd[3]) == 0) && (server_port[l] == atoi(cmd[2])))
					{
						for (m=0;m<no_of_neighbours;m++)
						{
							if (server_id [l] == neighbour_id[k])
							{
								strcpy(incoming_status,neighbour_status[k]);
							}
						}
					}
				}

				// HANDLING UPDATE INITIATED ON THE OTHER SERVER

				if (strcasecmp(cmd[0],"UPDATE") == 0)
				{
					for (l=0; l<no_of_neighbours && (max_cmd_2 == 4) && (atoi(cmd[2]) == this_server);l++)
					{
						if (atoi(cmd[1]) == neighbour_id[l])
						{
							strcpy(cost[l],cmd[3]);
							errcheck = 10;
						}
					}
					if (errcheck == 10)
					{
						for (l = 0; l < no_of_servers; l++)
						{
							for (j = 0; j < no_of_neighbours; j++)
							{
								if (server_id[l] == neighbour_id[j])
								{
									strcpy(cost_to_server[l], cost[j]);
									break;
								}
							}
						}

						printf("UPDATE SUCCESSFUL\n");

						// SEND UPDATE MSG BACK TO OTHER SERVER

						strcpy(buffer,"Updated on the other server");
						sendto(i, buffer, strlen(buffer), 0, (struct sockaddr *)&n_server, sizeof(n_server));
						break;
					}

					// UPDATE ERROR HANDLING

					else
					{
						printf("UPDATE FAILED\n");
					}
				}

				// HANDLING A ROUTING UPDATE

				else if ((strcasecmp(cmd[0],"DV") == 0) && (crash_status != 1) && (strcasecmp(incoming_status,"E") == 0))
				{
					dv_algorithm = 1;

					for (l=0;l<no_of_servers;l++)
					{
						if((atoi(cmd[2]) == server_port[l]) && (strcasecmp(cmd[3],server_ip[l]) == 0))
						{
							sender_server_id = server_id[l];
						}
					}

					printf("\nRouting Update arrived from %d %s(%d) \n",sender_server_id,inet_ntoa(n_server.sin_addr),ntohs(n_server.sin_port));

					// CREATING THE COST MATRIX FROM THE ROUTING UPDATE RECEIVED

					for (l=7;l<max_cmd_2;l=l+5)
					{
						strcpy(calculation[sender_server_id-1].hop_cost[atoi(cmd[l])-1],cmd[l+1]);
						sprintf(calculation[sender_server_id-1].no_of_hops[atoi(cmd[l])-1], "%d" ,atoi(cmd[l]));
					}

					for (l=0;l<no_of_servers;l++)
					{
						strcpy(calculation[this_server-1].hop_cost[server_id[l]-1],cost_to_server[l]);
						sprintf(calculation[this_server-1].no_of_hops[server_id[l]-1], "%d" ,hop_value[l]);
					}

					for (l=0;l<no_of_servers;l++)
					{
						for (m=0;m<no_of_servers;m++)
						{
							if (strlen(calculation[l].hop_cost[m]) == 0)
							{
								strcpy(calculation[l].hop_cost[m],"inf");
							}
						}
					}
					j=0;
					k=0;
					l=0;
					no_of_packets++;
				}

				// HANDLING OTHER MESSAGES FROM THE OTHER SERVERS

				else if ((strcasecmp(cmd[0],"DV") != 0) && (strcasecmp(cmd[0],"UPDATE") != 0))
				{
					printf("MSG RECEIVED : %s\n", buffer);
					gettimeofday(&select_return, NULL);
					break;
				}
			}
			gettimeofday(&select_return, NULL);
			continue;
		}

		// RUNNING THE DISTANCE-VECTOR ALGORITHM

		while(dv_algorithm != 0)
		{
			dv_algorithm = 0;
			dv_copy_all = 1;
			for (j=0;j<no_of_servers;j++)
			{
				for (k=0;k<no_of_servers;k++)
				{
					for (l=0;l<no_of_servers;l++)
					{
						if (strcasecmp(calculation[j].hop_cost[k],"inf") == 0)
						{
							strcpy(calculation[j].hop_cost[k],"50000");
						}
						if (strcasecmp(calculation[j].hop_cost[l],"inf") == 0)
						{
							strcpy(calculation[j].hop_cost[l],"50000");
						}
						if (strcasecmp(calculation[l].hop_cost[k],"inf") == 0)
						{
							strcpy(calculation[l].hop_cost[k],"50000");
						}
						if (atoi(calculation[j].hop_cost[k]) > atoi(calculation[j].hop_cost[l]) + atoi(calculation[l].hop_cost[k]))
						{
							// SEARCHING FOR THE SHORTEST PATH TO DESTINATION
							least_cost_a_b = atoi(calculation[j].hop_cost[l]) + atoi(calculation[l].hop_cost[k]);
							sprintf(calculation[j].hop_cost[k], "%d", least_cost_a_b);
							// STORING THE NEXT ROUTER
							sprintf(calculation[j].next_router[k], "%d", server_id[l]);
							// STORING THE NUMBER OF HOPS
							least_cost_a_b = atoi(calculation[j].no_of_hops[l]) + atoi(calculation[l].no_of_hops[k]);
							sprintf(calculation[j].no_of_hops[k], "%d", least_cost_a_b);
							dv_algorithm++;
						}
						// IF SAME INDEX THEN NO NEXT ROUTER
						if (j == k)
						{
							strcpy(calculation[j].next_router[k],"-");
						}
					}
				}
			}
		}

		// UPDATE THE TOPOLOGY DATA AFTER DISTANCE VECTOR ALGORITHM HAS RUN

		for (j=0;j<no_of_servers && dv_copy_all == 1;j++)
		{
			if (server_id[j] == this_server)
			{
				for (k=0;k<no_of_servers;k++)
				{
					if (strcmp(calculation[j].hop_cost[server_id[k]-1], "50000") == 0)
					{
						strcpy(cost_to_server[k],"inf");
						hop_value[k] = 0;
					}
					else
					{
						strcpy(cost_to_server[k],calculation[j].hop_cost[server_id[k]-1]);
						hop_value[k] = atoi(calculation[j].no_of_hops[server_id[k]-1]);
					}
				}
			}
		}
		dv_copy_all = 0;
		j=0;
		k=0;

		// SENDING ROUTING UPDATES TO THIS SERVER'S NEIGHBOURS

		if (packet_sender == 0 && crash_status != 1)
		{
			printf("\n");

			sprintf(m_update_fields, "%d", no_of_servers);
			sprintf(m_server_port,"%d",this_server_port);
			strcpy(m_server_ip,myip());

			// CREATING A HEADER FOR THE OTHER SERVERS TO IDENTIFY THE ROUTING UPDATE

			strcpy(message, "DV ");

			// STORING THE MESSAGE IN THE SENDING BUFFER

			strcpy(message, strcat(message, m_update_fields));
			strcpy(message, strcat(message, " "));
			strcpy(message, strcat(message, m_server_port));
			strcpy(message, strcat(message, "\n"));
			strcpy(message, strcat(message, m_server_ip));
			strcpy(message, strcat(message, "\n"));

			for (i=0;i<no_of_servers;i++)
			{
				strcpy(message, strcat(message, server_ip[i]));
				strcpy(message, strcat(message, "\n"));

				sprintf(m_server_port,"%d",server_port[i]);
				strcpy(message, strcat(message, m_server_port));
				strcpy(message, strcat(message, " "));

				sprintf(m_hop,"%d",hop_value[i]);
				strcpy(message, strcat(message, m_hop));
				strcpy(message, strcat(message, "\n"));

				sprintf(m_server_id,"%d",server_id[i]);
				strcpy(message, strcat(message, m_server_id));
				strcpy(message, strcat(message, " "));

				strcpy(message, strcat(message, cost_to_server[i]));
				strcpy(message, strcat(message, "\n"));
			}

			// SENDING THE ROUTE UPDATE TO THE ENABLED NEIGHBOURS

			for (i=0;i<no_of_neighbours;i++)
			{
				for (j=0;j<no_of_servers;j++)
				{
					if ((neighbour_id[i] == server_id[j]) && (strcasecmp(neighbour_status[i],"E") == 0))
					{
						n_server.sin_family = AF_INET;
						n_server.sin_addr.s_addr = inet_addr(server_ip[j]);
						n_server.sin_port = htons(server_port[j]);

						sendto(l_sock, message, strlen(message), 0, (struct sockaddr *)&n_server, sizeof(n_server));
						printf("DV sent to %d...\n",server_id[j]);
					}
				}
			}
		}
		gettimeofday(&select_return, NULL);

	}

	// CLOSING THE UDP SOCKET

	close(l_sock);

	// RETURN '0' : PROGRAM RAN WITHOUT ERRORS

	return (0);
}

// END OF CODE
