#include "notifier.h"
#include <iostream>
#include <fstream>
#include <string>
using std::string;
using std::ifstream;

void LedNotifier::parse(const char* parseFileName){
	ifstream fin(parseFileName);
	if(!fin){
		throwGeneralFailure();
	}
	while(fin.good()){
		string name;
		int green0in;
		int green1in;
		int yellow0in;
		int yellow1in;
		int red0in;
		int red1in;
		fin >> name >> green0in>>green1in>>yellow0in >> yellow1in >> red0in >> red1in;
		//Stupid way to convert these two booleans but yolo
		bool green0 = (green0in == 1);
		bool green1 = (green1in == 1);
		bool yellow0 = (yellow0 == 1);
		bool yellow1 = (yellow1 == 1);
		bool red0 = (red0in == 1);
		bool red1 = (red1in == 1);
		notificationMap[name] = LedArray(green0,green1,yellow0,yellow1,red0,red1);
	}
}	

bool LedNotifier::throwLedCode(string code, bool throwGeneralErrorOnFailure){
	if(notificationMap.find(code) == notificationMap.end()){
		if(throwGeneralErrorOnFailure)
			throwGeneralFailure();
		return false;
	}
	LedArray ledArray = notificationMap[code];
	lightLeds(ledArray.green0,ledArray.green1,ledArray.yellow0,ledArray.yellow1,ledArray.red0,ledArray.red1);
	return true;
}


//lightLeds sends a message to our arduino telling it which lights to turn on.
//It accepts 6 booleans, one for each of the lights.
//For example if green0 (the first argument) is true then the first green light will be turned on

//this function could be called as follows with object led_notif (of LedNotifier class):

// led_notif.lightLeds(0,1,0,1,1,0); 
// this would turn on the second green light, second yellow light, and first red light
void LedNotifier::lightLeds(bool green0, bool green1, bool yellow0, bool yellow1, bool red0, bool red1){
	
	
	//create a byte, where we will ignore the first two bits.
	//each digit of the byte will represent one of our six lights
	//if a digit is a 1, that light will be told to turn on
	//for example xx000000 will represent all lights being off
	//whereas xx010001 will represent g1 and r1 being on.
	byte lightbyte = 0x00; 
	
	//create a message that we will use to send the byte to the topic
	std_msgs::byte msg;
	
	//if loops check our bools, if any of them are true change the corresponding of the byte to a 1 instead of a 0
	if(green0) //first green light
	{
		lightbyte += 0x20; //add byte xx100000
	}
	if(green1) //second green light
	{
		lightbyte += 0x10; //add byte xx010000
	}
	if(yellow0) //first yellow light
	{
		lightbyte += 0x08; //add byte xx001000 
	}
	if(yellow1) //second yellow light
	{
		lightbyte += 0x04; //add byte xx000100
	}
	if(red0) //first red light
	{
		lightbyte += 0x02; //add byte xx000010
	}
	if(red1) //secoond red light
	{
		lightbyte += 0x01; //add byte xx000001
	}
	
	//set our message byte to be equal to the byte we made
	msg.data = lightbyte;
	
	//publish the message we just set onto our topic
	pub.publish(msg);
	
	//spin once to wait properly
	ros::spinOnce();
	
	return;
}
LedNotifier::LedNotifier(bool parseOnConstruction){
	
	ros::init(0,NULL,"lednotifier");
	pub = nh.advertise<std_msgs::byte>("/master/leds", 1000);
	
	if(parseOnConstruction)
		parse();
}


