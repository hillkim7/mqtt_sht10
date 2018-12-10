#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <mqtt.h>
#include <cjson/cJSON.h>
#include "posix_sockets.h"
#include <libconfig.h>

#define RESULT_SUCCESS 0
#define RESULT_FAIL -1

#define FILENAME "/root/mqtt_config.cfg"

const char* addr;
const char* port;
char topic[20];
int time_delay_ms = 1000;

/**
 * @brief The function that would be called whenever a PUBLISH is received.
 * 
 * @note This function is not used in this example. 
 */
void publish_callback(void** unused, struct mqtt_response_publish *published);

/**
 * @brief The client's refresher. This function triggers back-end routines to 
 *        handle ingress/egress traffic to the broker.
 * 
 * @note All this function needs to do is call \ref __mqtt_recv and 
 *       \ref __mqtt_send every so often. I've picked 100 ms meaning that 
 *       client ingress/egress traffic will be handled every 100 ms.
 */
void* client_refresher(void* client);

/**
 * @brief Safelty closes the \p sockfd and cancels the \p client_daemon before \c exit. 
 */
void exit_example(int status, int sockfd, pthread_t *client_daemon);


int read_setting(const char* filename){
	config_t cfg;
	config_setting_t *setting;
	const char *str;
	char buf[100];

	config_init(&cfg);

	if(!config_read_file(&cfg, filename))
	{
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
		config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return RESULT_FAIL;
	}

	if(!config_lookup_string(&cfg, "time_server", &str))
	{
		fprintf(stderr, "Setting 'time_server' read error\n");
		return RESULT_FAIL;
	}
		
	if(!config_lookup_int(&cfg, "time_delay", &time_delay_ms))
	{
		fprintf(stderr, "Setting 'time_delay' read error\n");
		return RESULT_FAIL;
	}
	
	setting = config_lookup(&cfg, "internet.wifi");
	if(setting != NULL)
	{
		config_setting_t *wifi = config_setting_get_elem(setting, 0);
		const char *ip, *netmask, *gateway;
		int is_dhcp;
		if(!(config_setting_lookup_int(wifi, "is_dhcp", &is_dhcp) 
			&& config_setting_lookup_string(wifi, "ip", &ip)
			&& config_setting_lookup_string(wifi, "netmask", &netmask)
			&& config_setting_lookup_string(wifi, "gateway", &gateway)))
		{
				fprintf(stderr, "Read 'internet -> wifi' setting error\n");
				return RESULT_FAIL;
		}
	}

	setting = config_lookup(&cfg, "internet.ethernet");
	if(setting != NULL)
	{
		config_setting_t *ethernet = config_setting_get_elem(setting, 0);
		const char *ip, *netmask, *gateway;
		int is_dhcp;
		if(!(config_setting_lookup_int(ethernet, "is_dhcp", &is_dhcp) 
			&& config_setting_lookup_string(ethernet, "ip", &ip)
			&& config_setting_lookup_string(ethernet, "netmask", &netmask)
			&& config_setting_lookup_string(ethernet, "gateway", &gateway)))
		{
				fprintf(stderr, "Read 'internet -> ethernet' setting error\n");
				return RESULT_FAIL;
		}
	}

	setting = config_lookup(&cfg, "mqtt");
	if(setting != NULL)
	{
		const char *service_code, *node_id;
		if(!(config_setting_lookup_string(setting, "server", &addr) 
			&& config_setting_lookup_string(setting, "service_code", &service_code)
			&& config_setting_lookup_string(setting, "node_id", &node_id)
			&& config_setting_lookup_string(setting, "port", &port)))
		{
				fprintf(stderr, "Read 'mqtt' setting error\n");
				return RESULT_FAIL;
		}
		
		sprintf(topic, "IoTF1/%s/%s/stat/_cur", service_code, node_id);
	}

//	config_destroy(&cfg);
	return RESULT_SUCCESS;
}

int get_epoch_time(){
	char output[100];
	char tmp[100];
	memset(tmp, 0x00, sizeof(tmp));
	FILE *p = popen("date +%s", "r");

	if(p != NULL) {
		while(fgets(output, sizeof(output), p) != NULL);
		strncpy(tmp, output, strlen(output)-1);
	}
	else{ 
		printf("read time error\n");
		pclose(p);
		return -1;
	}
	
	pclose(p);
	return atoi(tmp);
}

double get_temp(){
	char output[100];
	char tmp[100];
	memset(tmp, 0x00, sizeof(tmp));
	FILE *p = popen("get_temp", "r");

	if(p != NULL) {
		while(fgets(output, sizeof(output), p) != NULL);
		strncpy(tmp, output, strlen(output)-1);
	}
	else{ 
		printf("read temp error\n");
		pclose(p);
		return -1;
	}
	
	pclose(p);
	return atof(tmp);
}

double get_humi(){
	char output[100];
	char tmp[100];
	memset(tmp, 0x00, sizeof(tmp));
	FILE *p = popen("get_humi", "r");

	if(p != NULL) {
		while(fgets(output, sizeof(output), p) != NULL);
		strncpy(tmp, output, strlen(output)-1);
	}
	else{ 
		printf("read humi error\n");
		pclose(p);
		return -1;
	}
	
	pclose(p);
	return atof(tmp);
}

char *create_example(long utc_time, double temp_val, double humi_val){
	char *string = NULL;
	cJSON *wrap = cJSON_CreateObject();
	cJSON *temp = cJSON_CreateObject();
	cJSON *humi = cJSON_CreateObject();
 
	if (cJSON_AddNumberToObject(wrap, "tm", utc_time) == NULL)
	{        
		exit(EXIT_FAILURE);    
	}

	cJSON_AddItemToObject(wrap, "temp", temp);
	if (cJSON_AddNumberToObject(temp, "val", temp_val) == NULL)
	{
		exit(EXIT_FAILURE);    	
	}
	if (cJSON_AddStringToObject(temp, "unit", "C") == NULL)
	{
		exit(EXIT_FAILURE);    	
	}
	
	cJSON_AddItemToObject(wrap, "humi", humi);
 	if (cJSON_AddNumberToObject(humi, "val", humi_val) == NULL)
	{
		exit(EXIT_FAILURE);    	
	}
	if (cJSON_AddStringToObject(humi, "unit", "%") == NULL)
	{
		exit(EXIT_FAILURE);    	
	}

	string = cJSON_Print(wrap);	
	
	cJSON_Delete(wrap);
	return string;
}

/**
 * A simple program to that publishes the current time whenever ENTER is pressed. 
 */
int main(int argc, const char *argv[]) 
{


	if(read_setting(FILENAME) == RESULT_FAIL)
	{   
		exit(EXIT_FAILURE);
	}   

	printf("addr : %s, port : %s\n", addr, port);
	printf("topic %s\n", topic);

    /* open the non-blocking TCP socket (connecting to the broker) */
    int sockfd = open_nb_socket(addr, port);

    if (sockfd == -1) {
        perror("Failed to open socket: ");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    /* setup a client */
    struct mqtt_client client;
    uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
    uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    mqtt_connect(&client, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    /* start a thread to refresh the client (handle egress and ingree client traffic) */
    pthread_t client_daemon;

    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
        fprintf(stderr, "Failed to start client daemon.\n");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    /* start publishing the time */
    printf("%s is ready to begin publishing the time.\n", argv[0]);
    printf("Press ENTER to publish the current time.\n");
    printf("Press CTRL-D (or any other key) to exit.\n\n");

	while(1) {
		char *strtest = create_example(get_epoch_time(), get_temp(), get_humi());	
		

		/* publish the time */
		mqtt_publish(&client, topic, strtest, strlen(strtest) + 1, MQTT_PUBLISH_QOS_0);
		free(strtest);
		printf("Published to topic : %s\n", topic);
		usleep(time_delay_ms * 1000);

		/* check for errors */
		if(client.error != MQTT_OK){
			fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
			exit_example(EXIT_FAILURE, sockfd, &client_daemon);
		}
	}   

    /* disconnect */
    printf("\n%s disconnecting from %s\n", argv[0], addr);
    sleep(1);

    /* exit */ 
    exit_example(EXIT_SUCCESS, sockfd, &client_daemon);
}

void exit_example(int status, int sockfd, pthread_t *client_daemon)
{
    if (sockfd != -1) close(sockfd);
    if (client_daemon != NULL) pthread_cancel(*client_daemon);
    exit(status);
}

void publish_callback(void** unused, struct mqtt_response_publish *published) 
{
}

void* client_refresher(void* client)
{
    while(1) 
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}
