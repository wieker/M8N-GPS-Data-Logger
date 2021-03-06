/*
	GPS_UBLOX.cpp - Ublox GPS library for Arduino
	Code by Jordi Mu�oz and Jose Julio. DIYDrones.com
	This code works with boards based on ATMega168 / 328 and ATMega1280/2560 (Serial port 1)

		This library is free software; you can redistribute it and / or
		modify it under the terms of the GNU Lesser General Public
		License as published by the Free Software Foundation; either
		version 2.1 of the License, or (at your option) any later version.

	GPS configuration : Ublox protocol
		Baud rate : 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, or 115200

	Active messages : 
		NAV - PVT
		
	Methods:
		Init() : GPS Initialization
		Read() : Call this funcion as often as you want to ensure you read the incomming gps data
		
	Properties in use:
		Year :
		Month :
		Day :
		Hour :
		Min :
		Sec :
		Fix : 2-3 = GPS FIX, 0 = No Fix (normal logic)
		NumSV ; Number of satelites
		Longitude : Longitude * 10,000,000 (long value)
		Lattitude : Lattitude * 10,000,000 (long value)
		Altitude :	Altitude * 100 (meters) (long value)
		Ground_speed : Speed (m / s) * 100 (long value)
		NewData : 1 when a new data is received.
		You need to write a 0 to NewData when you read the data

*/

#include "GPS_UBLOXPVT.h"
#include "Arduino.h"

// Constructors ////////////////////////////////////////////////////////////////
GPS_UBLOXPVT_Class::GPS_UBLOXPVT_Class()
{
}

// Public Methods //////////////////////////////////////////////////////////////
void GPS_UBLOXPVT_Class::Init(void)
{
	ck_a = 0;
	ck_b = 0;
	UBX_step = 0;
	NewData = 0;
	Fix = 0;
	GPS_timer = millis();	 // Restarting timer...
	Serial.begin(115200); //300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, or 115200
}

// optimization : This code don�t wait for data, only proccess the data available
// We can call this function on the main loop (50Hz loop)
// If we get a complete packet this function calls parse_ubx_gps() to parse and update the GPS info.

void GPS_UBLOXPVT_Class::Read(void)
{
	static unsigned long GPS_timer = 0;
	byte data;
	int numc;
	
	numc= Serial.available();

	if (numc > 0)
		for (int i = 0; i < numc; i++)	// Process bytes received
			{
		     	data = Serial.read();
			switch(UBX_step)		 // Normally we start from zero. This is a state machine
			{
			case 0:	
				if(data == 0xB5){	// UBX sync char 1
					UBX_step++;	 // OH first data packet is correct, so jump to the next step
				}
				break; 

			case 1:	
				if(data == 0x62){	// UBX sync char 2
					UBX_step++;	 // ooh! The second data packet is correct, jump to the step 2
				}
				else 
					UBX_step = 0;	 // Nop, is not correct so restart to step zero and try again.		 
				break;

			case 2:
				UBX_class = data;
				ubx_checksum(UBX_class);
				UBX_step++;
				break;

			case 3:
				UBX_id = data;
				ubx_checksum(UBX_id);
				UBX_step++;
				break;

			case 4:
				UBX_payload_length_hi = data;
				ubx_checksum(UBX_payload_length_hi);
				UBX_step++;
		
		// We check if the payload lenght is valid...
			if (UBX_payload_length_hi >= UBX_MAXPAYLOAD)
				{
				UBX_step = 0;	 // Bad data, so restart to step zero and try again.		 
				ck_a = 0;
				ck_b = 0;
				}
				break;
			case 5:
				UBX_payload_length_lo = data;
				ubx_checksum(UBX_payload_length_lo);
				UBX_step++;
		UBX_payload_counter = 0;
				break;

			case 6:				 // Payload data read...
				if (UBX_payload_counter < UBX_payload_length_hi)	// We stay in this state until we reach the payload_length
				{
					UBX_buffer[UBX_payload_counter] = data;
					ubx_checksum(data);
					UBX_payload_counter++;
					if (UBX_payload_counter == UBX_payload_length_hi)
					UBX_step++;
				}
				break;

			case 7:
				UBX_ck_a = data;	 // First checksum byte
				UBX_step++;
				break;

			case 8:
				UBX_ck_b = data;	 // Second checksum byte
			 
	  // We end the GPS read...
				if((ck_a == UBX_ck_a) && (ck_b == UBX_ck_b))	 // Verify the received checksum with the generated checksum.. 
				parse_ubx_gps();							 // Parse the new GPS packet
				else
			{
			//Errors, something could be done
			}
        // Variable initialization
				UBX_step = 0;
				ck_a = 0;
				ck_b = 0;
				GPS_timer = millis(); // Restarting timer...
				break;
		}
		}		// End for 2 seconds If we don�t receive GPS packets in 2 seconds => Bad FIX state
	if ((millis() - GPS_timer) > 2000)
		{
	Fix = 0;
		}
}

/****************************************************************
 * 
 ****************************************************************/
// Private Methods //////////////////////////////////////////////////////////////
void GPS_UBLOXPVT_Class::parse_ubx_gps(void)
{
	int j;
//Verifing if we are in class 1, you can change this "IF" for a "Switch" in case you want to use other UBX classes.. 
//In this case all the message im using are in class 1, to know more about classes check PAGE 60 of DataSheet.
	if(UBX_class == 0x01) 
	{
		switch(UBX_id) //Checking the UBX ID
		{

		case 0x07: // ID NAV - PVT 
			j = 0;
	//Time = join_4_bytes(&UBX_buffer[j]); // ms Time of week
			j += 4;
			Year = join_2_bytes(&UBX_buffer[j]); // Year
			j += 2;
			Month = UBX_buffer[j]; // Month
			j += 1;
			Day = UBX_buffer[j]; //Day
			j += 1;
			Hour = UBX_buffer[j]; //Hour
			j += 1;
			Min = UBX_buffer[j]; //Minute
			j += 1;
			Sec = UBX_buffer[j];	//Seconds
			j += 1;
	//valid = UBX_buffer[j];	//Validity Flag
			j += 1;
	//tAcc = join_4_bytes(&UBX_buffer[j]); //Time Accuracy
			j += 4;
	//nano = join_4_bytes(&UBX_buffer[j]); //Fraction of second
			j += 4;
			Fix = UBX_buffer[j];	//Fix type(0 No fix, 1 Dead Reckoning, 2 2D, 3 3D, 4 GNSS +Dead Recknoning, 5 time only
			j += 1;
	//flags = UBX_buffer[j]; //Fix status flags
			j += 1;
	//reserved_1  = UBX_buffer[j];  //Reserved, not used
			j += 1;
			numSV = UBX_buffer[j]; //Satelites in view
			j += 1;
			Longitude = join_4_bytes(&UBX_buffer[j]); // lon * 10000000
			j += 4;
			Lattitude = join_4_bytes(&UBX_buffer[j]); // lat * 10000000
			j += 4;
	//height = join_4_bytes(&UBX_buffer[j]); // Altitude over ellipsoid
			j += 4;
			Altitude = join_4_bytes(&UBX_buffer[j]); // Altitude over sealevel
			j += 4;
//hacc = join_4_bytes(&UBX_buffer[j]); //Horizontal accuracy
			j += 4;
//vacc = join_4_bytes(&UBX_buffer[j]); //Vertical accuracy
			j += 4;
//velN = join_4_bytes(&UBX_buffer[j]); //Velocity North
			j += 4;
	//valE = join_4_bytes(&UBX_buffer[j]); //Velocity East
			j += 4;
//velD = join_4_bytes(&UBX_buffer[j]); //Velocity down
			j += 4;
			Ground_Speed = join_4_bytes(&UBX_buffer[j]); // Speed in mm/s
			j += 4;
	//headmot = join_4_bytes(&UBX_buffer[j]); //Heading motion 2D
			j += 4;
	//sacc = join_4_bytes(&UBX_buffer[j]); //Speed accuracy
			j += 4;
	//headAcc = join_4_bytes(&UBX_buffer[j]); //Heading accuracy
			j += 4;
	//pdop = join_2_bytes(&UBX_buffer[j]); //Position DOP

	//A few Bytes are skipped here. (Reserved bytes and Heading of vehicle(2D)), see Ublox spec if needed

			NewData = 1;
			break;

		}
	}	 
}

/****************************************************************
 * 
 ****************************************************************/
 // Join 4 bytes into a long
long GPS_UBLOXPVT_Class::join_4_bytes(unsigned char Buffer[])
{
	union long_union {
	int32_t dword;
	uint8_t	byte[4];
} longUnion;

	longUnion.byte[0] = *Buffer;
	longUnion.byte[1] = *(Buffer + 1);
	longUnion.byte[2] = *(Buffer + 2);
	longUnion.byte[3] = *(Buffer + 3);
	return(longUnion.dword);
}

/****************************************************************
 * 
 ****************************************************************/
// Join 2 bytes into a long
short GPS_UBLOXPVT_Class::join_2_bytes(unsigned char Buffer[])
{
	union short_union {
	int16_t dword;
	uint8_t	byte[2];
} shortUnion;

	shortUnion.byte[0] = *Buffer;
	shortUnion.byte[1] = *(Buffer + 1);
	return(shortUnion.dword);
}


/****************************************************************
 * 
 ****************************************************************/
// Ublox checksum algorithm
void GPS_UBLOXPVT_Class::ubx_checksum(byte ubx_data)
{
	ck_a += ubx_data;
	ck_b += ck_a;
}

GPS_UBLOXPVT_Class GPS;

