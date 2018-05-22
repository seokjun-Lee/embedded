#include <string.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <time.h>
#include <math.h>

pthread_cond_t empty1, empty2, fill1, fill2;
pthread_mutex_t mutex1, mutex2;

#define MAX 100

int loops = 100;
int buffer1[MAX];
int buffer2[MAX];
int fill_ptr1 = 0;
int use_ptr1 = 0;
int count1 = 0;
int fill_ptr2 = 0;
int use_ptr2 = 0;
int count2 = 0;

#define MAXTIMINGS 85

int ret_temp;
int temp_count = 0;

#define FAN	22
#define CS_MCP3208	8
#define SPI_CHANNEL	0
#define SPI_SPEED 1000000
#define RGBLEDPOWER	24

#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "demofarmdb"

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

void sig_handler(int signo);

static int DHTPIN = 11;

static int dht22_dat[5] = {0,0,0,0,0};

int read_mcp3208_adc(unsigned char adcChannel){
	unsigned char buff[3];
	int adcValue = 0;

	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
	buff[1] = ((adcChannel & 0x07) << 6);
	buff[2] = 0x00;

	digitalWrite(CS_MCP3208, 0);
	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

	buff[1] = 0x0f & buff[1];
	adcValue = (buff[1] << 8) | buff[2];

	digitalWrite(CS_MCP3208, 1);

	return adcValue;
}

static uint8_t sizecvt(const int read)
{
  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

int read_dht22_dat()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);

  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40);

  pinMode(DHTPIN, INPUT);

  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

   // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) &&
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;

        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

		ret_temp = (int)t;

    return ret_temp;
  }
  else
  {
    printf("Data not good, skip\n");
    return 0;
  }
}

void fanon(int ret_temp){
	pinMode(FAN, OUTPUT);
	if(ret_temp >= 28){
		temp_count++;
		printf("temp~ING COUNT : %d\n", temp_count);
		if(temp_count >= 5)
			digitalWrite(FAN, 1);
	}
	else{
		digitalWrite(FAN, 0);
		temp_count = 0;
	}
}

void ledon(int adcValue){
	pinMode(RGBLEDPOWER, OUTPUT);

	if(adcValue >= 1000)
		digitalWrite(RGBLEDPOWER, 1);
	else
		digitalWrite(RGBLEDPOWER, 0);
}

void put_temp(int received_temp)
{
	buffer1[fill_ptr1] = received_temp;
	fill_ptr1 = fill_ptr1 + 1;

	count1++;
}

int get_temp(){
	int received_temp;

	read_dht22_dat();
	received_temp = ret_temp;
	printf("temp = %d\n", received_temp);

	fanon(received_temp);
	int tmp = buffer1[use_ptr1];
	use_ptr1 = use_ptr1 + 1;
	count1--;

	delay(1000);

	return tmp;
}

void put_light(int adcValue_light)
{
	buffer2[fill_ptr2] = adcValue_light;
	fill_ptr2 = fill_ptr2 + 1;

	count2++;
}

int get_light()
{
	unsigned char adcChannel_light = 0;
	int adcValue_light = 0;

	adcValue_light = read_mcp3208_adc(adcChannel_light);
	printf("light = %d\n", adcValue_light);

	ledon(adcValue_light);
	int lgt = buffer2[use_ptr2];
	use_ptr2 = use_ptr2 + 1;
	count2--;

	delay(1000);

	return lgt;
}

void *put_into_db(void *arg)
{
		unsigned char adcChannel_light = 0;
		int received_temp = 0;
		int adcValue_light = 0;
		int i;

	for(i=0; i<loops; i++){
		read_dht22_dat();
		adcValue_light = read_mcp3208_adc(adcChannel_light);
		received_temp = ret_temp;

		printf("-----------------------%d\t%d\n", received_temp, adcValue_light);
		char query[1024];

		sprintf(query, "insert into thl2 values (now(), %d, %d);", received_temp, adcValue_light);

		if(mysql_query(connector, query)){
			fprintf(stderr, "%s\n", mysql_error(connector));
			printf("Write DB error\n");
		}
		delay(10000);
	}
}

void *temp(void *arg)
{
	int i;
	for(i=0; i<loops; i++){
		pthread_mutex_lock(&mutex1);

		while(count1 == MAX)
			pthread_cond_wait(&empty1, &mutex1);

		put_temp(i);
		pthread_cond_signal(&fill1);
		pthread_mutex_unlock(&mutex1);
	}
}

void *light(void *arg)
{
	int i;
	for(i=0; i<loops; i++){
		pthread_mutex_lock(&mutex2);

		while(count2 == MAX)
			pthread_cond_wait(&empty2, &mutex2);
		put_light(i);
		pthread_cond_signal(&fill2);
		pthread_mutex_unlock(&mutex2);
	}
}

void *fan(void *arg)
{
	int i;
	for(i=0; i<loops; i++){
		pthread_mutex_lock(&mutex1);
		while(count1 == 0)
			pthread_cond_wait(&fill1, &mutex1);
		int tmp = get_temp();

		pthread_cond_signal(&empty1);
		pthread_mutex_unlock(&mutex1);
	}
}

void *led(void *arg)
{
	int i;
	for(i=0; i<loops; i++){
		pthread_mutex_lock(&mutex2);
		while(count2 == 0)
			pthread_cond_wait(&fill2, &mutex2);
		int lgt = get_light();

		pthread_cond_signal(&empty2);
		pthread_mutex_unlock(&mutex2);
	}
}

int main (void)
{
	signal(SIGINT, (void *)sig_handler);

	connector = mysql_init(NULL);
	if(!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0)){
		fprintf(stderr, "%s\n", mysql_error(connector));
		return 0;
	}
	printf("MySQL opened.\n");

	if(wiringPiSetup() == -1)
	{
		fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
		return 1;
	}

	if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
	{
		fprintf(stdout, "wiringPiSPISetup Failed : %s\n", strerror(errno));
		return 1;
	}
	pinMode(CS_MCP3208, OUTPUT);

	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;

	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}
	while (read_dht22_dat() == 0)
	{
		delay(500); // wait 1sec to refresh
	}

	pthread_t p1, p2, p3, p4, p5;

	pthread_create(&p1, NULL, temp, NULL);
	pthread_create(&p2, NULL, light, NULL);
	pthread_create(&p3, NULL, fan, NULL);
	pthread_create(&p4, NULL, led, NULL);
	pthread_create(&p5, NULL, put_into_db, NULL);

	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
	pthread_join(p3, NULL);
	pthread_join(p4, NULL);
	pthread_join(p5, NULL);

	mysql_close(connector);

	return 0;
}

void sig_handler(int signo)
{
	printf("process stop\n");
	digitalWrite(FAN, 0);
	digitalWrite(RGBLEDPOWER, 0);

	exit(0);
}
