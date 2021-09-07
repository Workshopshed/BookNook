/*****************************************************************************
* | File      	:   epd2in02d_V2.cpp
* | Author      :   Waveshare team, Andy from Workshopshed
* | Function    :   1.02inch e-paper V2
* | Info        :
*----------------
* |	This version:   V2.0
* | Date        :   2019-06-24
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include <stdlib.h>
#include <avr/pgmspace.h>

#include "epd1in02d_V2.h"

/**
 * full screen update LUT
**/
const unsigned char lut_w1[] =
{
0x60	,0x5A	,0x5A	,0x00	,0x00	,0x01	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
 	
};	
const unsigned char lut_b1[] =
{
0x90	,0x5A	,0x5A	,0x00	,0x00	,0x01	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,


};
/**
 * partial screen update LUT
**/
const unsigned char lut_w[] =
{
0x60	,0x01	,0x01	,0x00	,0x00	,0x01	,
0x80	,0x0f	,0x00	,0x00	,0x00	,0x01	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,

};	
const unsigned char lut_b[] =
{
0x90	,0x01	,0x01	,0x00	,0x00	,0x01	,
0x40	,0x0f	,0x00	,0x00	,0x00	,0x01	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,

};

Epd::~Epd()
{
};

Epd::Epd()
{
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
};

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command)
{
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data)
{
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void Epd::WaitUntilIdle(void)
{
    while(1) {      //LOW: idle, HIGH: busy
        if(DigitalRead(busy_pin) == 0)
            break;
        DelayMs(100);
    }
}

/******************************************************************************
function :	LUT download
******************************************************************************/
void Epd::SetFulltReg(void)
{
	unsigned int count;
	SendCommand(0x23);
	for(count=0;count<42;count++)	     
	{SendData(lut_w1[count]);}    
	
	SendCommand(0x24);
	for(count=0;count<42;count++)	     
	{SendData(lut_b1[count]);}          
}

/******************************************************************************
function :	LUT download
******************************************************************************/
void Epd::SetPartReg(void)
{
	unsigned int count;
	SendCommand(0x23);
	for(count=0;count<42;count++){
		SendData(lut_w[count]);
	}
	
	SendCommand(0x24);
	for(count=0;count<42;count++){
		SendData(lut_b[count]);
	}          
}


int Epd::Init(char Mode)
{
  if (IfInit() != 0) {
        return -1;
  }

 	Reset(); 

	SendCommand(0xD2);			
	SendData(0x3F);
						 
	SendCommand(0x00);  			
	SendData (0x6F);  //from outside

	SendCommand(0x01);  //power setting
	SendData (0x03);	    
	SendData (0x00);
	SendData (0x2b);		
	SendData (0x2b); 
	
  SendCommand(0x06);  //Configuring the charge pump
	SendData(0x3f);
	
	SendCommand(0x2A);  //Setting XON and the options of LUT
	SendData(0x00); 
	SendData(0x00); 
	
	SendCommand(0x30);  //Set the clock frequency
	SendData(0x13); //50Hz

	SendCommand(0x50);  //Set VCOM and data output interval
	SendData(0x57);			

  SendCommand(0x60);  //Set The non-overlapping period of Gate and Source.
	SendData(0x22);

  if (Mode == FULL) {
	
	SendCommand(0x61);  //resolution setting
	SendData (0x50);    //source 128 	 
	SendData (0x80);       

	SendCommand(0x82);  //sets VCOM_DC value
	SendData(0x12);  //-1v

	SendCommand(0xe3);//Set POWER SAVING
	SendData(0x33);
	SetFulltReg();	
  
  } else {

  //TODO: Check of we can merge these last few vales
  SendCommand(0x82);  //Set VCOM_DC value
  SendData(0x12);//-1v

  SendCommand(0xe3);//Set POWER SAVING
  SendData(0x33);

  SetPartReg(); 
    
  }

  SendCommand(0x04);//Set POWER ON

  WaitUntilIdle();
	return 0;
}

/******************************************************************************
function :	Turn On Display
******************************************************************************/
void Epd::RefreshDisplay(void)
{
    //These power on/off cause it to lock up
    //SendCommand(0x04);  //power on
    //WaitUntilIdle();
    SendCommand(0x12);  //Start refreshing the screen
    DelayMs(10);
    WaitUntilIdle();
    //SendCommand(0x02);
    //WaitUntilIdle();    //power off
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void)
{
  //Timings adjusted according to https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_1in02d.c
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
    DigitalWrite(reset_pin, LOW);                //module reset
    DelayMs(2);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
}

void Epd::Clear(void)
{
    int w, h;
    w = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    h = EPD_HEIGHT;
	  SendCommand(0x10);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            SendData(0x00);
        }
    }
	  SendCommand(0x13);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            SendData(0xff);
        }
    }

	RefreshDisplay();
}


void Epd::Display(const unsigned char* frame_buffer)
{
    int w = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    int h = EPD_HEIGHT;

    SendCommand(0x10);
    for (int j = 0; j < EPD_HEIGHT; j++) {
          for (int i = 0; i < w; i++) {
              SendData(0xff);
          }
    }

    if (frame_buffer != NULL) {
        SendCommand(0x13);
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
//May need to invert the data here. Seems to be white on black.              
                SendData(~pgm_read_byte(&frame_buffer[i + j * w]));
            }
        }
    }

    RefreshDisplay();
}

void Epd::DisplayPartBaseImage(const unsigned char* frame_buffer)
{
/* Set partial Windows */
    SendCommand(0x91);		//This command makes the display enter partial mode
    SendCommand(0x90);		//resolution setting
    SendData(0);           //x-start
    SendData(79);       //x-end

    SendData(0);
    SendData(127);  //y-end
    SendData(0x00);

    int w = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    int h = EPD_HEIGHT;

    if (frame_buffer != NULL) {
        SendCommand(0x10);
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                SendData(pgm_read_byte(&frame_buffer[i + j * w]));
            }
        }

        SendCommand(0x13);
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                SendData(pgm_read_byte(&frame_buffer[i + j * w]));
            }
        }
    }

    /* Set partial refresh */
    RefreshDisplay();
}

void Epd::DisplayPart(const unsigned char* frame_buffer)
{
    int w = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    int h = EPD_HEIGHT;

    if (frame_buffer != NULL) {
        SendCommand(0x13);
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                SendData(pgm_read_byte(&frame_buffer[i + j * w]));
            }
        }
    }

    /* Set partial refresh */
    RefreshDisplay();
}

void Epd::ClearPart(void)
{
    int w, h;
    w = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    h = EPD_HEIGHT;
    SendCommand(0x13);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            SendData(0xff);
        }
    }

     /* Set partial refresh */
    RefreshDisplay();
}

/**
 *  @brief: After this command is transmitted, the chip would enter the
 *          deep-sleep mode to save power.
 *          The deep sleep mode would return to standby by hardware reset.
 *          The only one parameter is a check code, the command would be
 *          executed if check code = 0xA5.
 *          You can use Epd::Init() to awaken
 */
void Epd::Sleep()
{
    SendCommand(0X02);  	//power off
    WaitUntilIdle();
    SendCommand(0X07);  	//deep sleep
    SendData(0xA5);
    DelayMs(200);

    DigitalWrite(reset_pin, LOW);
}

/* END OF FILE */


