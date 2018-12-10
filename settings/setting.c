#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <libconfig.h>
#include <string.h>
		
#define RESULT_FAIL	-1
#define RESULT_SUCCESS	0

static struct option long_options[] =
{
	{"time_server", required_argument, 0, 'a'},

	{"wifi_dhcp", required_argument, 0, 'b'},
	{"wifi_ip", required_argument, 0, 'c'},
	{"wifi_netmask", required_argument, 0, 'd'},
	{"wifi_gateway", required_argument, 0, 'e'},

	{"eth_dhcp", required_argument, 0, 'f'},
	{"eth_ip", required_argument, 0, 'g'},
	{"eth_netmask", required_argument, 0, 'h'},
	{"eth_gateway", required_argument, 0, 'i'},
	
	{"mqtt_server",   required_argument,       0, 'j'},
	{"mqtt_port",   required_argument,       0, 'k'},
	{"mqtt_code",   required_argument,       0, 'l'},
	{"mqtt_id",   required_argument,       0, 'm'},
	{"mqtt_interval", required_argument, 0, 'n'},
	{0, 0, 0, 0}
};

int is_dhcp_eth = 1, is_dhcp_wifi = 1;
const char *ip_eth, *netmask_eth, *gateway_eth;
const char *ip_wifi, *netmask_wifi, *gateway_wifi;
const char *time_server;

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

	if(!config_lookup_string(&cfg, "time_server", &time_server))
	{
		fprintf(stderr, "Setting 'time_server' read error\n");
		return RESULT_FAIL;
	}
	
	
	setting = config_lookup(&cfg, "internet.wifi");
	if(setting != NULL)
	{
		config_setting_t *wifi = config_setting_get_elem(setting, 0);
		if(!(config_setting_lookup_int(wifi, "is_dhcp", &is_dhcp_wifi) 
			&& config_setting_lookup_string(wifi, "ip", &ip_wifi)
			&& config_setting_lookup_string(wifi, "netmask", &netmask_wifi)
			&& config_setting_lookup_string(wifi, "gateway", &gateway_wifi)))
		{
				fprintf(stderr, "Read 'internet -> wifi' setting error\n");
				return RESULT_FAIL;
		}
	}

	setting = config_lookup(&cfg, "internet.ethernet");
	if(setting != NULL)
	{
		config_setting_t *ethernet = config_setting_get_elem(setting, 0);
		if(!(config_setting_lookup_int(ethernet, "is_dhcp", &is_dhcp_eth) 
			&& config_setting_lookup_string(ethernet, "ip", &ip_eth)
			&& config_setting_lookup_string(ethernet, "netmask", &netmask_eth)
			&& config_setting_lookup_string(ethernet, "gateway", &gateway_eth)))
		{
				fprintf(stderr, "Read 'internet -> ethernet' setting error\n");
				return RESULT_FAIL;
		}
	}

	//config_destroy(&cfg);
	return RESULT_SUCCESS;
}


#define CONFIG_FILENAME	"/etc/network/interfaces"
#define ETH0_HOTPLUG	"allow-hotplug eth0\n"
#define ETH0_DHCP		"iface eth0 inet dhcp\n"
#define ETH0_STATIC	"iface eth0 inet static\n"


int write_ethernet_config(int is_dhcp, const char* ip, const char* netmask, const char* gateway)
{
	char buf[200];

	printf("ip : %s\n", ip);

	if(is_dhcp == 1)
	{
		sprintf(buf, "echo '%s %s' >> %s", ETH0_HOTPLUG, ETH0_DHCP, CONFIG_FILENAME);
		system(buf);
	}
	else
	{
		sprintf(buf, "echo '%s %s address %s\nnetmask %s\ngateway %s\n' >> %s", ETH0_HOTPLUG, ETH0_STATIC, ip, netmask, gateway, CONFIG_FILENAME);
		system(buf);		
	}

	return 0;
}

#define WIFI_HOTPLUG	"auto wlan0\n"
#define WIFI_DHCP		"iface wlan0 inet dhcp\n"
#define WIFI_STATIC	"iface wlan0 inet static\n"
#define WIFI_WPA		"wpa-roam /etc/wpa_supplicant/wpa_supllicant.conf"
int write_wifi_config(int is_dhcp, const char* ip, const char* netmask, const char* gateway)
{
	char buf[200];

	if(is_dhcp == 1)
	{
		sprintf(buf, "echo '%s %s %s' >> %s", WIFI_HOTPLUG, WIFI_DHCP, WIFI_WPA, CONFIG_FILENAME);
		system(buf);
	}
	else
	{
		sprintf(buf, "echo '%s %s\n address %s\nnetmask %s\ngateway %s\n%s\n' >> %s", WIFI_HOTPLUG, WIFI_STATIC, ip, netmask, gateway, WIFI_WPA, CONFIG_FILENAME);
		system(buf);		
	}

	return 0;
}


int main(int argc, char *argv[])
{
	int opt;
	char buf[150];
	config_t cfg;
	config_setting_t *setting;	
	const char* filename = "/root/mqtt_config.cfg";
	
#if 0
	int is_dhcp_eth = 1, is_dhcp_wifi = 1;
	const char *ip_eth, *netmask_eth, *gateway_eth;
	const char *ip_wifi, *netmask_wifi, *gateway_wifi;
	const char *time_server;
#endif
	config_init(&cfg);
	if(!config_read_file(&cfg, filename))
	{
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
		config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return RESULT_FAIL;
	}

	while((opt = getopt_long(argc, argv, "a:b:c:d:e:f:g:h:i:j:k:l:m:", long_options, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a':	//time server
				printf("Time Server : %s\n", optarg);
				setting = config_lookup(&cfg, "time_server");
   			config_setting_set_string(setting, optarg);
				break;

			case 'b':	//wifi is dhcp?
				printf("WIFI_DHCP = %s\n", optarg);
				is_dhcp_wifi = atoi(optarg);
				setting = config_lookup(&cfg, "internet.wifi.is_dhcp");
   			config_setting_set_int(setting, atoi(optarg));
				break;
			case 'c':	//wifi ip
				printf("Wi-Fi IP : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.wifi.ip");
   			config_setting_set_string(setting, optarg);
				break;
			case 'd':	//wifi netmask
				printf("Wi-Fi Netmask : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.wifi.netmask");
   			config_setting_set_string(setting, optarg);
				break;
			case 'e':	//wifi gateway
				printf("Wi-Fi Gateway : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.wifi.gateway");
   			config_setting_set_string(setting, optarg);
				break;
		
			case 'f':	//ethernet is dhcp?
				printf("ETH0_DHCP = %s\n", optarg);
				is_dhcp_eth = atoi(optarg);
				setting = config_lookup(&cfg, "internet.ethernet.is_dhcp");
   			config_setting_set_int(setting, atoi(optarg));
				break;
			case 'g':	//Ethernet ip
				printf("Ethernet IP : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.ethernet.ip");
   			config_setting_set_string(setting, optarg);
				break;
			case 'h':	//Ethernet netmask
				printf("Ethernet Netmask : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.ethernet.netmask");
   			config_setting_set_string(setting, optarg);
				break;
			case 'i':	//Ethernet gateway
				printf("Ethernet Gateway : %s\n", optarg);
				setting = config_lookup(&cfg, "internet.ethernet.gateway");
   			config_setting_set_string(setting, optarg);
				break;

			case 'j':	//MQTT Server Name
				printf("MQTT Server : %s\n", optarg);
				setting = config_lookup(&cfg, "mqtt.server");
   			config_setting_set_string(setting, optarg);
			case 'k':	//MQTT Port
				printf("MQTT Port : %s\n", optarg);
				setting = config_lookup(&cfg, "mqtt.port");
   			config_setting_set_string(setting, optarg);
			case 'l':	//MQTT Service Code
				printf("MQTT Service Code : %s\n", optarg);
				setting = config_lookup(&cfg, "mqtt.service_code");
   			config_setting_set_string(setting, optarg);
			case 'm':	//MQTT Node ID
				printf("MQTT Node ID : %s\n", optarg);
				setting = config_lookup(&cfg, "mqtt.node_id");
   			config_setting_set_string(setting, optarg);
			case 'n':	//MQTT Report Interval
				printf("MQTT Report Interval : %s\n", optarg);
				setting = config_lookup(&cfg, "time_delay");
   			config_setting_set_int(setting, atoi(optarg));

		}
	}
	
	if(! config_write_file(&cfg, filename))
   {   
      fprintf(stderr, "Error while writing file.\n");
      config_destroy(&cfg);
      return(EXIT_FAILURE);
   }   
	config_destroy(&cfg);
	printf("Config file update success\n");

	
	if(read_setting(filename) == RESULT_FAIL)
   {   
      exit(EXIT_FAILURE);
   }	

	sprintf(buf, "echo 'source /etc/network/interfaces.d/*\nauto lo\niface lo inet loopback\n' > %s", CONFIG_FILENAME);
	system(buf);

	printf("ip : %s\n", ip_eth);

	write_ethernet_config(is_dhcp_eth, ip_eth, netmask_eth, gateway_eth);
	write_wifi_config(is_dhcp_wifi, ip_wifi, netmask_wifi, gateway_wifi);

	sprintf(buf, "rdate -s %s", time_server);
	system(buf);

	printf("Config Update Success. Reboot in 5 Seconds....\n");
	usleep(4200000);
	system("reboot now");

	return 0;
}
