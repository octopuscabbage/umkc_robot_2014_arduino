#include <QuadEncoder.h>

/* mega movement tester
 * written by: victoria wu
 * date: 1/17/14 
 *
 * what: make the robot keep going until the gap is found. then stop. (going to put it all on the mega, no bridge just yet
 * FIRST - just getting the robot to move. NO comms between mini and mega. just everything on the mega, ignoring encoders right now
 * PURPOSE: 
 
 * .... pro tip: if you get the error "serial2 is not declared in this scope" check that the board you're programming for IS THE MEGA
 * -> first starting. .the motors just go crazy  a bit 
 
 * next -> turn in place until parallel!! 
 */

//woohoo ros time !!!
#include <ros.h>
#include <mega_caretaker/MegaPacket.h>
#include "redux_mega_packet_defs.h"

#include "navigation.h"

#include <AnalogDistanceSensor.h>
#include <Distance2D120X.h>
#include <DistanceSensor.h>

#include <gapfinder.h>
#include <irsensor_tester.h>
#include <magellan_edgesensors.h>
#include <fronteyes.h>

#include <parallelpark_simple.h>
#include <motor_cmd.h>

#include <movement.h>
#include "top_level_state.h"  //silly arduino doesn't let me use enum as parameter unless it's in another header file T.T


Navigation nav;
int gapsThru;


state_top current_status;



    
 //ros meta
bool ros_control;  //is ros in control?
 
ros::NodeHandle  nh;	
mega_caretaker::MegaPacket temp;
    
 
    //pubs and subscribers
ros::Publisher talker("arduinoToBoard", &temp);
void packet_catch(const mega_caretaker::MegaPacket& packet);  
ros::Subscriber<mega_caretaker::MegaPacket> listener("boardToArduino", &packet_catch);

//callback
      

//ros msg catching time!
void packet_catch(const mega_caretaker::MegaPacket& packet)  {
    if(packet.msgType == MSGTYPE_HEY)  {
        if(packet.payload == PL_START_WAVE_CROSSING)  {
	    ros_control = false;
            current_status =  start; 
            //gotta put out an ack T.T
            sendAck();
            
        }
    }
    else if(packet.msgType == MSGTYPE_ACK)  {
        if(packet.payload == PL_FINISHED_TURNING_90)  {
	    current_status = theend;
	    ros_control = false;
            sendAck();
        }
    }
  
    else if(packet.msgType == MSGTYPE_MOTORCOM)  {
        if(packet.payload == PL_STOP)  {
          //STOP STOP STOP!!!
          nav.stopNow();  //may need to write in more robust code to keep stopping until...???
        }
        else if (packet.payload == PL_TURNCW)  {
          nav.turnClockwiseForever();
        }
    }
}

void sendAck()  {
  temp.msgType = MSGTYPE_ACK;
  temp.payload = PL_GENERAL_ACK;
  talker.publish(&temp);
}

void advertiseState(state_top now)  {
  temp.msgType = MSGTYPE_STATE;
  switch(now)    {
    case waiting:
		temp.payload = PL_WAITING;
		break;
	case start:
		temp.payload = PL_START;
		break;
	case turningCW_init:
		temp.payload = PL_TURNING_CW_INIT;
		break;
	case turningCW_wait:
		temp.payload = PL_TURNING_CW_WAIT;
		break;
	case turningCW_fin:
		temp.payload = PL_TURNING_CW_FIN;
		break;
  	case theend:
		temp.payload = PL_END;
		break;
  }
  talker.publish(&temp);

}

void initROS()  {
  nh.initNode();
  nh.advertise(talker);
  nh.subscribe(listener);

}

void sendMsg_finishedWaveCrossing()  {
  temp.msgType = MSGTYPE_ACK;
  temp.payload = PL_FINISHED_WAVE_CROSSING;
  talker.publish(&temp);
}

void initiateTurn90()  {
    temp.msgType = MSGTYPE_HEY;
    temp.payload = PL_START_TURNING_90;
    talker.publish(&temp);
}



void setup() {
	Serial.begin(9600);
	nav.init();
        current_status = waiting;
        gapsThru = 0;
        ros_control = true;
        initROS();
}

void loop() {
        
         nh.spinOnce();
        //advertiseState(current_status);  //at this point it will FLOOD the channels, but i just want to hear something from you mega!!!!2   
        switch(current_status)  {
          case waiting:
            //looooooooop test forever
            //stay here FOREVER until you hear stuff from board
            break;
          case start:
	    //lets just test the turning 90
	    current_status = turningCW_init;
	    break;
          case turningCW_init:
            //time to ask ros board for help
			
	    initiateTurn90();
	    ros_control = true; //assume that board heard us the first time
	    current_status = turningCW_wait; 
          case turningCW_wait:
            //stay here until ros returns an answer we're good		
			
            break;
          case theend:
            //sendMsg_finishedWaveCrossing();
           break;
          
        
        }
  
  
  /*
        switch (current_status) {
          case start:
            //let's keep going
            //console.println("start \t go forward!");
            //
            Serial.println("start!");
            delay(5000);
            //current_status = moving;
             current_status = realignParallel;
            //current_status = crossingwave;
            break;
          case moving:
            Serial.println("state moving");
            nav.takeOff();
            if(nav.lookingForGap())  {
              Serial.println("GAP FOUND!");
              current_status = gapfound;
              delay(300);
            }
            //nav.traveling(); //this kind of works. not really :(
            break;
          case gapfound:
            //console.println("gapfound \t ");
			//now to turn 90 degrees
            Serial.println("starting 90 deg turn");
            nav.turnTowardsGap();
            delay(300);
            current_status = crossingwave;
//              current_status = theend;
            break;
          case crossingwave:
            
             if(nav.crossGap())  {
               gapsThru++;
               if(gapsThru ==2)  {
                 current_status = theend;
               }
               else  {                   
                 current_status = realignParallel ; 
               }
               delay(300);
//               current_status = theend;
             }
            break;
          case realignParallel:
            Serial.println("realignparalell");
             delay(300);
             //nav.parallelpark();
             
             //let's try with ros imu stuff yay
             
             //nav.turn90();
             
             //delay(1000);
             Serial.println("its parallel!");
             //current_status = gapfound_pt2;
             
             //current_status = moving;
              //send request to board...
                  //going nto hand over control to ros
                  initiateTurn90();  //hand control over to ROS
                  
                  //board itself will tell motors to GO or to STOP
                  //will send an OK 
                  
                  //gotta wait until i hear back an OK from the ros board
                  while(ros_control) {
                    //spin
                  }              
                  //hey i heard back!!
                  //im done.
         
             current_status = theend;
             //nav.sleep();       
          break;
          case theend:
            nav.sleep();
            nav.sleep();
          if (Serial.available() > 0) {
		// process incoming commands from console
		const byte cmd = Serial.read();
		switch(cmd) {
			case 'r':
				Serial.println("again!");
				//sabertooth.forward(40);
                                current_status = start;
			//	sabertooth.forward();
				break;
			
		
		}
	}	//
            
            break;
            
        
        }
        
       
       */ 
          
        //delay(50);  //make it readable
        //gapfind.printDebug();
      
      
      /*
  Serial.print("FL: ");
  Serial.println(positionFL);
  Serial.print(" BL: ");
  Serial.println(positionBL);
  Serial.print(" FR: ");
  Serial.println(positionFR);
  Serial.print(" BR: ");
  Serial.println(positionBR);  
     */  
     
}


