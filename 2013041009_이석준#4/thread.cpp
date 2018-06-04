#include <stdio.h>
#include <pthread.h>

//1
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <mysql/mysql.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
//1

#define MAX 10

//2
#define CS_MCP3208	8			//BCM_GPIO_8
#define SPI_CHANNEL	0
#define SPI_SPEED	1000000		//1MHz
#define VCC			4.8			//Supply Voltage

#define MAXTIMINGS	85
#define RETRY		5
//2

int buffer[MAX];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;

pthread_cond_t empty, fill;
pthread_mutex_t mutex;
int loops = 1000;

//3
int ret_temp;
static int DHTPIN = 11;
static int dht22_dat[5] = {0, 0, 0, 0, 0};
int received_temp;

#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "demofarmdb"

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;
//3

int read_mcp3208_adc(unsigned char adcChannel){
	unsigned char buff[3];
	int adcValue = 0;

	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
	buff[1] = ((adcChannel & 0x07) << 6);
	buff[2] = 0x00;

    digitalWrite(CS_MCP3208, 0);  // Low : CS Active

    wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

    buff[1] = 0x0F & buff[1];
    adcValue = (buff[1] << 8) | buff[2];

    digitalWrite(CS_MCP3208, 1);  // High : CS Inactive

    return adcValue;
}

static uint8_t sizecvt(const int read){
	if(read>255 || read<0){
		printf("Invalid data from wiringPi liberary\n");
		exit(EXIT_FAILURE);
	}
}

int read_dht22_dat_temp(){
	uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;

    dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

    // pull pin down for 18 milliseconds
    pinMode(DHTPIN, OUTPUT);
    digitalWrite(DHTPIN, HIGH);
    delay(10);
    digitalWrite(DHTPIN, LOW);
    delay(18);
    // then pull it up for 40 microseconds
    digitalWrite(DHTPIN, HIGH);
    delayMicroseconds(40);
    // prepar

    pinMode(DHTPIN, INPUT);

	// detect change and read data
	for(i=0; i< MAXTIMINGS; i++){
		counter = 0;
	    while(sizecvt(digitalRead(DHTPIN)) == laststate){
	    	counter++;
	    	delayMicroseconds(1);
	    	if(counter == 255){
	    		break;
	    	}
	    }
	    laststate = sizecvt(digitalRead(DHTPIN));

        if(counter == 255) break;

        // ignore first 3 transitions
        if((i>=4) && (i%2==0)){
	        // shove each bit into the storage bytes
	        dht22_dat[j/8] <<= 1;
            if(counter > 50)
	            dht22_dat[j/8] |= 1;
	        j++;
	    }
	}

	// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	// print it out if data is good
	if((j >= 40) &&
	    (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))){
	        float t;

            t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
	        t /= 10.0;
	        if((dht22_dat[2] & 0x80) != 0)  t *= -1;

            ret_temp = (int)t;
            //printf("Temperature = %.2f *C \n", t );
            printf("Temperature = %d\n", ret_temp);

        return ret_temp;
	}
	else{
		printf("Data not good, skip\n");
		return 0;
	}
}

int get_temperature_sensor(){
//	int received_temp;
	int _retry = RETRY;

	DHTPIN = 11;

/*	if(wiringPiSetup() == -1)
		exit(EXIT_FAILURE);

	if(setuid(getuid()) < 0){
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}
*/
	while(read_dht22_dat_temp() == 0 && _retry--){
		delay(500);
	}
	received_temp = ret_temp;
	//printf("Temperature = %d\n", received_temp);

	return received_temp;
}

int wiringPicheck(){
	if(wiringPiSetup() == -1){
		fprintf(stdout, "Unable to start wiringPi : %s\n", strerror(errno));
		return 1;
	}
}

void put(int ret_temp){
    buffer[fill_ptr] = received_temp;
    fill_ptr = (fill_ptr + 1)/* % MAX*/;
    count++;
}

int get(){
    int tmp = buffer[use_ptr];
    //while(1){
        char query[1024];

        get_temperature_sensor();     //Temp Sensor

        sprintf(query, "insert into thl values (now(), %d);", ret_temp);

        if(mysql_query(connector, query)){
	        fprintf(stderr, "%s\n", mysql_error(connector));
            printf("Write DB error\n");
        }
        delay(3000);
    //}
    use_ptr = (use_ptr + 1)/* % MAX*/;
    count--;
    return tmp;
}

void *producer(void *arg){
    int i;

    for(i=0; i<loops; i++){
	    printf("%s : begin\n", (char *) arg);
    	pthread_mutex_lock(&mutex);

        while(count == MAX)
	        pthread_cond_wait(&empty, &mutex);

        put(i);
        pthread_cond_signal(&fill);
        pthread_mutex_unlock(&mutex);
        printf("Producer value : %d\n", buffer[i]);
        printf("%s : done\n", (char *) arg);
	}
}

void *consumer(void *arg){
    int i;

    for(i=0; i<loops; i++){
	    printf("%s : begin\n", (char *) arg);
        pthread_mutex_lock(&mutex);

        while(count == 0)
	        pthread_cond_wait(&fill, &mutex);

        int tmp = get();
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
        printf("Consumer value : %d\n", tmp);
        printf("%s : done\n", (char *) arg);
	}
}

int main(int argc, char *argv[]){
	int adcChannel = 0;
	int adcValue[8] = {0};

	if(wiringPiSetup() == -1)
		exit(EXIT_FAILURE);

	if(setuid(getuid())<0){
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	pinMode(CS_MCP3208, OUTPUT);

	//MySQL connection
	connector = mysql_init(NULL);
	if(!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0)){
	    fprintf(stderr, "%s\n", mysql_error(connector));
	    return 0;
	}

    printf("MySQL(rpidb) opened.\n");

	pthread_t p1, p2;
	pthread_create(&p1, NULL, producer, "Producer");
	pthread_create(&p2, NULL, consumer, "Consumer");

	pthread_join(p1, NULL);
	pthread_join(p2, NULL);

	mysql_close(connector);

	return 0;
}
