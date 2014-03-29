
//#define DEBUG_COMMS  //don't test sensor stuf! just the comms!
#define ITERATION 1  //how many gaps to cross.. for debugging
#define LASTGAP 1  //which one to stop at and do the hardcoded one
#define PAUSE_DURATION  100  //how many milliseconds between movements

/* mega movement tester
 * written by: victoria wu
 * date: 1/17/14 
 *
 * what: make the robot Fkeep going until the gap is found. then stop. (going to put it all on the mega, no bridge just yet
 * FIRST - just getting the robot to move. NO comms between mini and mega. just everything on the mega, ignoring encoders right now
 * PURPOSE: 
 
 * .... pro tip: if you get the error "serial2 is not declared in this scope" check that the board you're programming for IS THE MEGA
 * -> first starting. .the motors just go crazy  a bit 
 
 * next -> turn in place until parallel!! 
 */

//woohoo ros time !
#include <QuadEncoder.h>
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

#include "FiniteStateMachine.h"


//=========================
//TOP LEVEL DECLARATIONS
//=========================
//... this is terrible. please fix me
//i need nav declaration to be up here, but FSM declaration needs to go after the state stuff T.T
Navigation nav;
int gapsThru;
FSM stateMachine; //initialize state machine, start in state: waitForCommand
state_top current_status;




//hacky hacky hacky :(
bool handshakeOK;    //used to sync handshake for old state machine
bool turn90DegreeFinished;

bool crossBoard;
bool  commandGoToTools;
bool  isGapFound;
bool  isGapCrossed;
bool  isEdgeFound;



//ros meta
bool ros_control;  //is ros in control?

ros::NodeHandle  nh;	
mega_caretaker::MegaPacket temp;
mega_caretaker::MegaPacket advertising_state;  //specifically for advertising state



//pubs and subscribers
ros::Publisher talker("/mega_caretaker/arduinoToBoard", &temp);
void packet_catch(const mega_caretaker::MegaPacket& packet);  
ros::Subscriber<mega_caretaker::MegaPacket> listener("/mega_caretaker/boardToArduino", &packet_catch);

//callback


//=======================
//STATE things
//=======================

void updateROS_spin()  {
  nh.spinOnce();
}

//-----------
//initializeComms - this is for the three way handshake-ing
//-----------

State initializeComms = State(enterInitializeComms, updateInitializeComms, NULL);

void enterInitializeComms()  {
  nav.stopNow();
}

void updateInitializeComms()  {
  updateROS_spin();
  //no talking. just waiting for syn + ack
}


//-----
// waitForCommand = mega is sitting idly, waiting for command from board.
//-----
State waitForCommand = State(enterWaitForCommand, updateWaitForCommand, NULL);  //wait for command from board. either to go somewhere, or start wave crossing.
void enterWaitForCommand()  {

  advertising_state.payload = PL_WAITING;
  talker.publish(&advertising_state);

  nav.stopNow();
  //this is replaced by the board initiated three way handshake
}

void updateWaitForCommand()  {
  
  updateROS_spin();

}


State finishedGoToTools = State(enterFinishedGoToTools, NULL, NULL);
void enterFinishedGoToTools()  {
  sendFinishedGoToTools();
  stateMachine.transitionTo(waitForCommand);
}


//----
//go to tools! (meta state)
//----

State goToTools = State(enterGoToTools, updateGoToTools, exitGoToTools);
void enterGoToTools()  {
  //this is a meta state??? bleeeeh
}
void updateGoToTools()  {
  //do the thing! do the thing get the tools the tools
  //say HEY im finished
  stateMachine.transitionTo(finishedGoToTools);
}
void exitGoToTools()  {

}




//----
//meta state - going from position to pick up tools, back to start position (hopefully just goingo backwards. may need to be more robust to
//account for straying too far from the board
//----
State transitionToolsToCrossBoard(enterTransitionToolsToCrossBoard, updateTransitionToolsToCrossBoard, exitTransitionToolsToCrossBoard);
void enterTransitionToolsToCrossBoard()  {
 advertising_state.payload = PL_TRANSITION_1_2;
 talker.publish(&advertising_state);
}

void updateTransitionToolsToCrossBoard()  {

}

void exitTransitionToolsToCrossBoard()  {
}
//-----
// waveCrossing = HEY start crossing waves mate! (meta state)
//-----

State crossingBoard = State(NULL, NULL, NULL);  //wait for command from board. either to go somewhere, or start wave crossing.

//------
//turn90Degrees = mega needs to turn 90 degrees now. requires help from board too.
//-------

State turn90Degrees_CW = State(enterTurn90Degrees_cw, updateTurn90Degrees, exitTurn90Degrees);
 void enterTurn90Degrees_cw()  {
 turn90DegreeFinished = false;
 advertising_state.payload = PL_TURNING_CW_INIT;
 talker.publish(&advertising_state);
 nav.stopNow();
 
 //Ask board for help!
 initiateTurn90_CW();
 }
 
 void updateTurn90Degrees()  {
 updateROS_spin();
 //update other code?? sensor code??
 }
 
 void exitTurn90Degrees()  {
 //?????????
 nav.stop_sleep(PAUSE_DURATION);
 //??????? do i need this???????????????????????????????//
 }
 
 State turn90Degrees_CCW = State(enterTurn90Degrees_ccw, updateTurn90Degrees, exitTurn90Degrees);
 void enterTurn90Degrees_ccw()  {
 turn90DegreeFinished = false;
 advertising_state.payload = PL_TURNING_CCW_INIT;
 talker.publish(&advertising_state);
 nav.stopNow();
 
 //Ask board for help!
 initiateTurn90_CCW();
 }
 
//----------
//GapFound -
//  -needs to turn 90degree CCW
//  -then go forward through the gap, stopping at a certain distance away
//  -then turn 90 degree CW back towards the lane
//----------

State gapFound = State(enterGapFound,NULL,NULL);
 void enterGapFound()  {
 advertising_state.payload = PL_GAP_FOUND;
 talker.publish(&advertising_state);
 
 }



//----------
//lookForGap - travel straight thru lanes, while looking for gap. Stop once we find a gap.
//----------

State lookForGap = State(enterLookForGap, updateLookForGap, exitLookForGap);
 void enterLookForGap()  {
 advertising_state.payload = PL_LOOKING_FOR_GAP;
 talker.publish(&advertising_state);
 nav.takeOff();
 }
 
 //ignoring falling off for now
 void updateLookForGap()  {
 //updateROS_spin();  //do i need this??
 #ifndef DEBUG_COMMS 
 isGapFound = nav.lookingForGap();
 //stateMachine.immediateTransitionTo(waitForCommand);    
 
 #endif
 #ifdef DEBUG_COMMS
 isGapFound = true;
     
 #endif
 }
 void exitLookForGap()  {
 nav.stop_sleep(PAUSE_DURATION);
 }
 
 //--------------
 //we are finished crossing the gap! in the next lane now.
 //--------------
 State gapCrossed = State(enterGapCrossed, updateGapCrossed, exitGapCrossed);
 void enterGapCrossed()  {
 advertising_state.payload = PL_GAP_CROSSED;
 talker.publish(&advertising_state);
 }
 void updateGapCrossed()  {
 
 }
 void exitGapCrossed()  {
 nav.stop_sleep(PAUSE_DURATION);
 
 }
 //-----------
 //crossGap - make the little crossing across the gap 
 //TODO TODO TODO - account for multiple crossings in a row
 //-------------
 State crossGap = State(enterCrossGap, updateCrossGap, exitCrossGap);
 void enterCrossGap()  {
 advertising_state.payload = PL_CROSSING_GAP;
 talker.publish(&advertising_state);
 gapsThru++;  //gapsThru is the nth gap you have crossed 
 
 }
 void updateCrossGap()  {
 //TODO TODO - account for ht elast one nooooooo
 #ifndef DEBUG_COMMS 
 if(gapsThru == LASTGAP) { //this is the third gap to cross
   //hardcode the ticks
   //block here block block
    nav.crossLastGap();  //blokc block block once its done it will have crossed hopefully
    isGapCrossed = true;
 }
 else  {
   isGapCrossed = nav.crossGap();
 }
 
 //stateMachine.transitionTo(waitForCommand);
 
 #endif
 #ifdef DEBUG_COMMS
 isGapCrossed = true;
 #endif
 
 }
 void exitCrossGap()  {
 nav.stop_sleep(PAUSE_DURATION);
 }
 
//-----------
//Finished wave crossing! yay
//-------------


State finishedCrossingBoard = State(enterFinishedCrossingBoard, updateFinishedCrossingBoard, exitCrossingBoard);
 void enterFinishedCrossingBoard()  {
 sendFinishedCrossingWaves();
 }
 void updateFinishedCrossingBoard()  {
 stateMachine.transitionTo(waitForCommand);
 
 }
 void exitCrossingBoard()  {
 
 }

//-----------
//findEdge- after crossing to another lane and turned clockwise.. go backwards and find edge. 
//-------------

State findEdge = State(enterFindEdge, updateFindEdge, exitFindEdge);
 void enterFindEdge()  {
 advertising_state.payload = PL_FINDING_EDGE;
 talker.publish(&advertising_state);
 nav.goBackwardForever();
 
 }
 
 void updateFindEdge()  {
   //TODO TODO - account for ht elast one nooooooo
   #ifndef DEBUG_COMMS
   isEdgeFound = nav.atEdge();//go "backwards" and find edge
   #endif
   #ifdef DEBUG_COMMS
   isEdgeFound = true;
   #endif
 }
 void exitFindEdge()  {
 
 nav.stop_sleep(PAUSE_DURATION);
 
 }
 




//=======================
//ALL THE ROS THINGS
//=======================      

//ros msg catching time!
void packet_catch(const mega_caretaker::MegaPacket& packet)  {
  //sendAck();  //sd;fjklasd;fljkasd;fkljasdfkl; jd you ack T.T
  
  if(packet.msgType == MSGTYPE_HEY)  {
    if(packet.payload == PL_START_WAVE_CROSSING)  {
      ros_control = false;
      //current_status =  start; 
      //gotta put out an ack T.T

      //CHANGE THIS !!!! only for testigngkgadjf;lasdkfjdl;askfj!
      //            start_wave_crossing = true;
      //            stateMachine.immediateTransitionTo(turn90Degrees_CW); 

      // 
      sendNonsense();
      crossBoard = true;
      // stateMachine.transitionTo(waitForCommand);

    }
    else if (packet.payload == PL_START_GO_TO_TOOLS)  {
      //go to do tools
      //what - when i transition to wait for command here... IT SENDS BACK A SYN ACK WHY
      //stateMachine.transitionTo(waitForCommand);
      commandGoToTools = true;
      
    }
  }
  
    else if(packet.msgType == MSGTYPE_ACK)  {
   if(packet.payload == PL_FINISHED_TURNING_90_CW || packet.payload == PL_FINISHED_TURNING_90_CCW) {
   	    //current_status = theend;
   	    ros_control = false;
   
   //new thing - immediate transition to finished state
           turn90DegreeFinished = true; 
   //stateMachine.immediateTransitionTo(waitForCommand); 
   
   
   }
   
   else if(packet.payload == PL_GENERAL_ACK)  {
   //this is a general ack... depdngin on what state we are in, we are ok
   }
   }
   
   else if(packet.msgType == MSGTYPE_MOTORCOM)  {
   if(packet.payload == PL_STOP)  {
   //STOP STOP STOP!
   nav.stopNow();  //may need to write in more robust code to keep stopping until...???
   
   }
   else if (packet.payload == PL_TURNCW)  {
   nav.turnClockwiseForever();
   
   }
   else if (packet.payload == PL_TURNCCW)  {
   nav.turnCounterClockwiseForever(); 
   }
   }
   
  else if(packet.msgType == MSGTYPE_HANDSHAKE)    {
    if(packet.payload == PL_SYN)  {

      // handshakeOK = false;
      // stateMachine.immediateTransitionTo(intializeComms);


      temp.msgType = MSGTYPE_HANDSHAKE;
      temp.payload = PL_SYN_ACK;
      talker.publish(&temp);

    }
    else if (packet.payload == PL_ACK)  {
      //connection with board established! 

      //stateMachine.immediateTransitionTo(waitForCommand);

      //current_state = start;
      handshakeOK = true;
    }
  }
}

void sendNonsense()  {
  temp.msgType = 99;
  temp.payload = 9999;
  talker.publish(&temp);
}

void sendAck()  {
  temp.msgType = MSGTYPE_ACK;
  temp.payload = PL_GENERAL_ACK;
  talker.publish(&temp);
}


void sendFinishedCrossingWaves()  {
 temp.msgType = MSGTYPE_ACK;
 temp.payload = PL_FINISHED_WAVE_CROSSING;
 talker.publish(&temp);
 }
 

void sendFinishedGoToTools()  {
  temp.msgType = MSGTYPE_ACK;
  temp.payload = PL_FINISHED_GO_TO_TOOLS;
  talker.publish(&temp);
}

void initROS()  {
  nh.initNode();
  nh.advertise(talker);
  nh.subscribe(listener);

  advertising_state.msgType = MSGTYPE_STATE;


}


void initiateTurn90_CW()  {
  temp.msgType = MSGTYPE_HEY;
  temp.payload = PL_START_TURNING_90_CW;
  talker.publish(&temp);
}


void initiateTurn90_CCW()  {
  temp.msgType = MSGTYPE_HEY;
  temp.payload = PL_START_TURNING_90_CCW;
  talker.publish(&temp);
}



//=======================
//MAIn stuff
//=======================




void setup() {
  Serial.begin(9600);
  nav.init();
  current_status = initComms;  //no synack, no rosssSS!
  gapsThru = 0;
  ros_control = true;
  initROS();
  stateMachine.init(initializeComms);

  handshakeOK = false;
  turn90DegreeFinished = false;

  crossBoard = false; 
  commandGoToTools = false;
  isGapFound = false;
  isGapCrossed = false;
  isEdgeFound = false;
  

}

void loop() {
  //nav.takeOff();
  //start out in waitForcommand.
  //state changes for now (at least) to go to turn90 are handled up there.

  //send the HEY I"m ready to be listening to stuff!!

  // nh.spinOnce();

  //state machine starts out in initializeComms. 
  //-> successfull three way handshake, it goes to 

  //have state machine transitions OUTSIDE here for my sanity


  nh.spinOnce();
  stateMachine.update();
  if(stateMachine.isInState(initializeComms))  {
    //wait and spin
    if(handshakeOK)  {
      stateMachine.transitionTo(waitForCommand);
    }
  }
  else if(stateMachine.isInState(waitForCommand))  {
    //spin
    //will set it to...crossingBoard state
     if(crossBoard)  {
       stateMachine.transitionTo(crossingBoard);
       crossBoard = false;
     } 
     else if (commandGoToTools)  {
       stateMachine.transitionTo(goToTools);
       commandGoToTools = false;
     }
  }


  
    //crossing board state ASSUMES we start from the beginning position     
   else if(stateMachine.isInState(crossingBoard))  {
   //spin
     gapsThru = 0;
      // stateMachine.transitionTo(waitForCommand);
     //stateMachine.transitionTo(finishedCrossingBoard);
     
     //THE VERY FIRST TIME - need to move all the way back to reset.. from looking for tools to going forward. 
     //future optimization - look for Gap 
     
     
     stateMachine.transitionTo(lookForGap);
   
   //  stateMachine.transitionTo(findEdge);
   
   
   //stateMachine.transitionTo(gapFound);
   }
   
   
   else if(stateMachine.isInState(lookForGap))  {
   //spin.. will transition to gapFound state when its' found
     if(isGapFound)  {
       
       //might need to hardcode the ticks if I'm not at an edge.
       
       
       isGapFound = false;
       stateMachine.transitionTo(gapFound);
     }
   }
   
   else if(stateMachine.isInState(gapFound))  {
   //now need to turn90degrees
   
   if(!nav.atEdge())  {
     nav.adjustToGap();
   }
   stateMachine.transitionTo(turn90Degrees_CCW);
   
   
   }
   
   else if (stateMachine.isInState(turn90Degrees_CCW)) {
   //spin... 
   if(turn90DegreeFinished)  {
   //stateMachine.transitionTo(waitForCommand);    //want to se if this works :(
   
   //gapsThru++;    //in position to cross yet another gap - if this is the 3rd one.. DON"T CROSS GAP just go forward and stop
   //if(gapsThru == 1)  {
   //  stateMachine.transitionTo(finishedCrossingBoard);
   //}
   //else  {
   // stateMachine.transitionTo(crossGap);   
   //}
   
     stateMachine.transitionTo(crossGap);
      turn90DegreeFinished = false;
     }
   }
   
   
   //stop here !!
   else if (stateMachine.isInState(crossGap))  {
   //spin
   //the update function in this state will transition once the gap is crossed and it ses a wave in front //TODO TODO update with sarahs code
     if(isGapCrossed)  {
       stateMachine.transitionTo(gapCrossed);
     }
   }
   else if (stateMachine.isInState(gapCrossed) )  {
       
       stateMachine.transitionTo(turn90Degrees_CW);
   
     
   //stateMachine.transitionTo(waitForCommand);
   
   }
   else if (stateMachine.isInState(turn90Degrees_CW))  {
   //spin
     if(turn90DegreeFinished)  {
   //and back to looking for gap
   //stateMachine.transitionTo(waitForCommand);
   //stateMachine.transitionTo(lookForGap);
   
   //-> go backwards until you hit an edge... then look for gap
       stateMachine.transitionTo(findEdge);
       turn90DegreeFinished = false;
   }
   }
   else if (stateMachine.isInState(findEdge))  {
   //???
   //... THIS IS SO hACKY D: -> TODO make it so magellans are active all the time?????? ndl;fkasjdfl;jkasd;fkljasfl;kasjdfkl;ajdfkl
   //spin
     //stateMachine.transitionTo(finishedCrossingBoard);
     if(isEdgeFound)  {
       if(gapsThru < ITERATION)  {
         stateMachine.transitionTo(lookForGap);
       }
       else  {
         stateMachine.transitionTo(finishedCrossingBoard);
       }
     }
   }
   
   
  /*
      
   //  case waveCrossing:
   //   stateMachine.immediateTransitionTo(lookForGap);  //this will spin until it finds a gap, when it goes immediately to next state
   // break;
   
   
  /*
   
   //old code for testing 90 degree with old state machine style OLD HORRIBLE STATE MACHINE STYLE DIIIIIE
   nh.spinOnce();
   //advertiseState(current_status);  //at this point it will FLOOD the channels, but i just want to hear something from you mega!F2   
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
   */



  /*
        switch (current_status) {
   case initComms:
   nh.spinOnce();
   //wait for syn, send syn-ack, wait for ack
   if(handshakeOK)  {
   current_status = start;
   }
   break;
   
   case start:
   //let's keep going
   //console.println("start \t go forward!");
   //
   Serial.println("start!");
   delay(5000);
   //current_status = moving;
   //current_status = realignParallel;
   current_status = crossingwave;
   break;
   case moving:
   Serial.println("state moving");
   nav.takeOff();
   if(nav.lookingForGap())  {
   Serial.println("GAP FOUND!");
   current_status = gapfound;
   //current_status = theend;
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
   current_status = theend;
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
   initiateTurn90_CW();  //hand control over to ROS
   
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
   				//sabertooth.reverse(40);
   current_status = start;
   			//	sabertooth.reverse();
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




