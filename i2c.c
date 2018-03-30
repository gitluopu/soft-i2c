
#include<stdio.h>

#define SDA_PIN AT91C_PIN_PD(19)
#define SCL_PIN AT91C_PIN_PD(20)

static inline void i2c_delay_bit(void) //i2c_baud 100kbit, here delay 1us
{
 int i = 10;
 while(i--);
}
static inline void i2c_sda_set(int val)
{
    at91_set_pio_value(SDA_PIN/32, SDA_PIN%32,val);
}

static inline void i2c_sda_in(void)
{
    at91_set_pio_input(SDA_PIN/32, SDA_PIN%32,0);
}
static inline void i2c_sda_out(int val)
{
    //at91_set_pio_multi_drive(SDA_PIN/32, SDA_PIN%32, 0);
    at91_set_pio_output(SDA_PIN/32, SDA_PIN%32, val);
}
static inline int i2c_sda_get(void)
{
    int val;
    val =  at91_get_pio_value(SDA_PIN/32, SDA_PIN%32);
	return val;
}

static inline void i2c_scl_out(int val)
{
    //at91_set_pio_multi_drive(SCL_PIN/32, SCL_PIN%32, 0);
    at91_set_pio_output(SCL_PIN/32, SCL_PIN%32, val);
}
static inline void i2c_scl_set(int val)
{
    at91_set_pio_value(SCL_PIN/32, SCL_PIN%32,val);
}

static inline void i2c_reset(void)
{
i2c_scl_set(0);
i2c_sda_set(1);
i2c_delay_bit();
}

static void i2c_init()
{
	i2c_sda_out(1);i2c_delay_bit();
	i2c_scl_out(1);i2c_delay_bit();
}

void i2c_start(void) 
{
    //prepare SDA level in case of repeated start
    //        i2c_scl(0);
    //        i2c_sda(1); i2c_delay_bit();
    //1-0 transition on SDA while SCL=1
    //i2c_sda_out();
    //i2c_init();
	i2c_sda_set(1);i2c_delay_bit();
    i2c_scl_set(1); i2c_delay_bit();
    i2c_sda_set(0); i2c_delay_bit();
    i2c_scl_set(0); i2c_delay_bit();
}

void i2c_stop(void) 
{
    //0-1 transition on SDA while SCL=1
    //i2c_scl_set(0); i2c_delay_bit();
	//i2c_sda_out();
	i2c_sda_set(0); i2c_delay_bit();
	i2c_scl_set(1); i2c_delay_bit();
	i2c_sda_set(1);i2c_delay_bit();
	i2c_scl_set(1); i2c_delay_bit();
}
void i2c_wr_bit(u8 b) 
{
    i2c_sda_set(b);i2c_delay_bit();
    i2c_scl_set(1); i2c_delay_bit();
    i2c_scl_set(0);i2c_delay_bit();
}
u8 i2c_rd_bit(void) 
{
	i2c_sda_in();
	i2c_sda_set(1); i2c_delay_bit();
    i2c_scl_set(1); i2c_delay_bit();
    u8 bit = i2c_sda_get();
	i2c_sda_out(bit);i2c_delay_bit();
	i2c_scl_set(0); i2c_delay_bit();
    return(bit);
}

u8 i2c_wr_byte(u8 b) 
{
	u8 i;
    for (i=8; i; i--) {
        i2c_wr_bit(b & 0x80);
        b <<= 1;
    }
    i2c_delay_bit();
    return(i2c_rd_bit()); //return ACK
}
u8 i2c_rd_byte(u8 ack)
{
    u8 result = 0;
	u8 i;
	
    for (i=8; i; i--) {
        result <<= 1;
        result |= !!(i2c_rd_bit());
    }

    i2c_wr_bit(ack);
    return(result);
}

//write--->INIT -- START --  SA+W CMD -A- D1 -A- D2 -A-  ... PEC  -A- STOP (**delay**)
//read --->START --  SA+W CMD -A- D1 -A- D2 -A-  ... PEC  -A- STOP 
//This board contains a PEC after every w/r procedure.write a command and stop,then must delay some time to
//make board process it and start it.
static u8 dvb_cmd_exec(dvb_cmd *cmd)
{
   u8 wr[DVB_CMD_MAX_LEN+3] = {0};
   u8 rd[DVB_CMD_MAX_LEN+3] = {0};
   int i;
   u8 ret;
   wr[0] = OD6000_I2C_WR_ADDRESS;
   wr[1] = cmd->id;
   for(i=0;i<cmd->para_cnt;i++){
   	wr[i+2] = cmd->para[i];
   	}
   wr[cmd->para_cnt+2] = crc8_msb(0x07,wr,cmd->para_cnt+2); //use crc8 poly 7 according to example
   i2c_init();
 i2c_start();
   for(i=0;i<cmd->para_cnt+3;i++){
   	if(i2c_wr_byte(wr[i]))
		LOGERR("dvb-i2c-write err.\n");
   	}
   i2c_stop();
   
 usleep(10);  //give od6000 time to process the cmd

 i2c_start();
if(i2c_wr_byte(OD6000_I2C_RD_ADDRESS))
		LOGERR("dvb-i2c-write err.\n");
rd[0] = OD6000_I2C_RD_ADDRESS;
 for(i=0;i<cmd->ret_cnt+2;i++){
   rd[i+1] = i2c_rd_byte(0);
 	}
 i2c_stop();
for(i=0;i<cmd->ret_cnt;i++)
LOGERR("rd[%d] = %x\n",i,rd[i]);
 ret = crc8_msb(0x07,rd,cmd->ret_cnt+2);
for(i=0;i<cmd->ret_cnt;i++)
	cmd->ret[i] = rd[i+2];
ret = rd[1];
if(rd[cmd->ret_cnt+2]!=crc8_msb(0x07,rd,cmd->ret_cnt+2))
	ret = 0x21;
 return ret;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char crc8_lsb(unsigned char poly, unsigned char* data, int size)
{
	unsigned char crc = 0x00;
	int bit;

	while (size--) {
		crc ^= *data++;
		for (bit = 0; bit < 8; bit++) {
			if (crc & 0x01) {
				crc = (crc >> 1) ^ poly;
			} else {
				crc >>= 1;
			}
		}
	}

	return crc;
}

unsigned char crc8_msb(unsigned char poly, unsigned char* data, int size)
{
	unsigned char crc = 0x00;
	int bit;

	while (size--) {
		crc ^= *data++;
		for (bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ poly;
			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

int main(void)
{
	unsigned char data[] = {0x40,0x65};
	unsigned char poly;

	// x8 + x4 + x3 + x2 + 1 -> 0x1D
	poly = 0x1D;
	printf("%02x\n", crc8_lsb(poly, data, sizeof(data)/sizeof(u8)));
	printf("%02x\n", crc8_msb(poly, data, sizeof(data)/sizeof(u8)));

	// x8 + x5 + x4 + 1 -> 0x31
	poly = 0x07;
	printf("%02x\n", crc8_lsb(poly, data, sizeof(data)/sizeof(u8)));
	printf("%02x\n", crc8_msb(poly, data, sizeof(data)/sizeof(u8)));

	return EXIT_SUCCESS;
}


