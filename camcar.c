//======================================================================
//
// Test program to test the infrared sensors (and motors) of the
// 4tronix initio robot car. One can run this program within an
// ssh session.
//
// author: Raimund Kirner, University of Hertfordshire
//         initial version: Dec.2016
//
// license: GNU LESSER GENERAL PUBLIC LICENSE
//          Version 2.1, February 1999
//          (for details see LICENSE file)
//
// Compilation: 
// gcc -o camcar -Wall -Werror -lcurses -lwiringPi -lpthread -linitio camcar.c
//
//======================================================================

#include <stdlib.h>
#include <initio.h>
#include <curses.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <pthread.h>
#include <assert.h>
#include "detect_blob.h"
#include <stdio.h>

//======================================================================
// Coursework ESD, general instructions
// This file (camcar.c) is the major file to be modified in order to 
// complete the coursework.  There are a few locations marked with "TODO",
// which indicate places where the code might need changes or completion.
// This directory also contains two other source files:
// quickblob.c ... this is a library for searching blobs in images
// detect_blob.c ... this is a wrapper for quickblob.c, providing a 
//                   direct interface to the RaspberryPI camera.
// Normally, quickblob.c and detect_blob.c don't need changes. However,
// studying detect_blob.c a bit is still advisable.
//
// The implementation of the nested state machine in camcar() follows
// the implementation proposal given in the Lecture slides. You may
// want to change the FSM implementation to add extra or refined
// behaviour.
//======================================================================

// desitance [in cm] to keep, given as two threshold values:
#define DIST_MIN 50
#define DIST_MAX 70

// data structure to communicate between main thread and camera thread
struct thread_dat {
  TBlobSearch blob;	// blob object from camera
  int blobnr;		// record blob nr
  int bExit; 		// flag to indicate termination of thread
};

pthread_mutex_t count_mutex; // mutex to protect thread communication

//======================================================================
// camcar():
// Skeleton code for the ESD coursework.
// The implementation uses hierarchical finite state machines (FSM), in order
// to reduce the size of the state transition graph.
// To be done: Fill the actions of the individual states of the FSMs
// with meaningful behaviour.
//======================================================================
void camcar(int argc, char *argv[], struct thread_dat *ptdat) 
{
	int SPEED = 50;
	int statusPin = 0;
	pinMode (37, OUTPUT);
    int ch = 0;
    int blobnr = 0;	// record blob nr of last movement
    TBlobSearch blob;	// blob object from camera thread
	delay(1000);
    // main control loop:  
    while (ch != 'q') {
        int obstacle_L, obstacle_R, obstacle; // FSM-OA
        int blobSufficient; // FSM-SB
        int carBlobAligned; // FSM-AB
        int distance;
        enum { distok, tooclose, toofar} distanceState; // FSM-MB

        mvprintw(1, 1,"%s: Press 'q' to end program", argv[0]);
        mvprintw(10, 1,"Status: blob(size=%d, halign=%f, blobnr=%u)  ", blob.size, blob.halign, ptdat->blobnr);

        obstacle_L = ( initio_IrLeft() !=0 );
        obstacle_R = ( initio_IrRight()!=0 );
        obstacle = obstacle_L || obstacle_R;

        // FSM-OA (Obstacle Avoidance)
        if (obstacle) {
            mvprintw(3, 1,"State OA (stop to avoid obstacle), o-left=%d, o-right=%d", obstacle_L, obstacle_R);
            clrtoeol(); // curses library
            initio_DriveForward (0); // Stop
        }
        else {
            refresh(); // curses lib: update display

            // TODO: thread communication: copy blob from camera thread (mutex protected)
			//lock mutex
			pthread_mutex_lock(&count_mutex);
			//
			blob = ptdat->blob; //read communication: copy blob from camera thread (mutex protected)
			//unlock mutex
			pthread_mutex_unlock(&count_mutex);
			
			
            // writeImageWithBlobAsJPEG() seems to have a bug, do not use right now:
            //writeImageWithBlobAsJPEG(blob, "test_blob.jpg", 70);  // this function is for testing (deactivate if not needed)
            blobSufficient = (blob.size > 200);  // TODO: experiment with that threshold value

            // FSM-SB (Search Blob)
            if ( ! blobSufficient ) {
                mvprintw(3, 1,"State SB (search blob), blob.size=%d (blobnr: %u)", blob.size, ptdat->blobnr);
                clrtoeol(); // curses library
                
                //if (blobSufficient < 2000 && blobSufficient > 1000)
                //{
				//	mvprintw(6, 1, "Blob is in sight, but not sufficient");
				//	initio_DriveForward(20); //drive until blob is of sufficient size
				//}
				
				
                if (blobnr < ptdat->blobnr) {
                    // TODO: potential actions: turn car or camera platform a few steps around and see if a blob is to be found
					statusPin = 1;
					initio_SpinRight(100);
					delay(200);
					initio_DriveForward(0);
					
                    blobnr = ptdat->blobnr;
                }
            } else {
				statusPin = 0;
                carBlobAligned = (blob.halign >= -0.25 && blob.halign <= 0.25);  // TODO: adjust values to useful ones // was -0.15
                

                // FSM-AB (Align to Blob)
                if ( ! carBlobAligned) {
                    mvprintw(3, 1,"State AB (align towards blob), blob.size=%d, halign=%f", blob.size, blob.halign);
                    clrtoeol(); // curses library
                    if (blobnr < ptdat->blobnr) {
                       // TODO: slightly turn car to align with blob (find a useful turn duration)
                       //       if blob.halign is negative, then turn right, otherwise left
                       if (blob.halign < 0)
                       {
							initio_SpinRight(100);
							delay(100);  // 200ms
							initio_DriveForward(0);
					   }
					   else
					   {
							initio_SpinLeft(100);
							delay(100);
							initio_DriveForward(0);
					   }
                       blobnr = ptdat->blobnr;
                    }
                } else { //if blob is alligned
                    distance = initio_UsGetDistance ();
                    if (distance < DIST_MIN)      { distanceState = tooclose; }
                    else if (distance > DIST_MAX) { distanceState = toofar; }
                    else                          { distanceState = distok; }
 
                    // FSM-MB (cat at middle of blob, keep distance)
                    switch (distanceState) {
                    case toofar:
                        mvprintw(3, 1,"State FB (drive forward), dist=%d", distance);
                        clrtoeol(); // curses library
                        // TODO: move car forward to come closer
						initio_DriveForward(SPEED);
                        break;
                    case tooclose:
                        mvprintw(3, 1,"State RB (drive backwards), dist=%d", distance);
                        clrtoeol(); // curses library
                        // TODO: move car backwards to get more distance
						initio_DriveReverse(SPEED);
                        break;
                    case distok:
                        mvprintw(3, 1,"State KD (keep distance), dist=%d", distance);
                        clrtoeol(); // curses library
                        initio_DriveForward (0); // Stop
                    } // switch (FSM-MB)
                } // if (FSM-AB)
            } // if (FSM-SB)
        } // if (FSM-OA)
		
		digitalWrite(37, statusPin);
        ch = getch();
        if (ch != ERR) mvprintw(2, 1,"Key code: '%c' (%d)  ", ch, ch);
        refresh(); // curses lib: update display
        //delay (100); // pause 100ms
  } // while
  statusPin = 0;
  return;
}


//======================================================================
// worker(): Thread function to continuously generate blob objects with camera
// This function will be executed by the explicitly created camera thread,
// to be executed concurrently with thcamerae main thread.
//======================================================================
void *worker(void *p_thread_dat)
{
  struct thread_dat *ptdat = (struct thread_dat *) p_thread_dat;
  const char blobColor[3] = { 255, 0, 0 };  // color to be detected as blob
  //const char blobColor[3] = {0, 0, 255};
  TBlobSearch blob;	// blob object from camera
  
  while (ptdat->bExit == 0) {
    blob = cameraSearchBlob( blobColor ); // search for sign with RED colored blob
    // TODO: fill in code: copy blob into shared data (mutex protected)

	//lock mutex
	pthread_mutex_lock(&count_mutex);
	//
	
	ptdat->blob = blob; //read communication: copy blob from camera thread (mutex protected)
	ptdat->blobnr++;
	//unlock mutex
	pthread_mutex_unlock(&count_mutex);

    
  } // while
  return NULL;
}


//======================================================================
// main(): initialisation of libraries, etc
//======================================================================
int main (int argc, char *argv[])
{
  WINDOW *mainwin = initscr();  // curses: init screen
  noecho ();                    // curses: prevent the key being echoed
  cbreak();                     // curses: disable line buffering 
  nodelay(mainwin, TRUE);       // curses: set getch() as non-blocking 
  keypad (mainwin, TRUE);       // curses: enable detection of cursor and other keys

  initio_Init (); // initio: init the library

  pthread_t cam_thread;         // pthread: thread handle
  pthread_attr_t pt_attr;       // pthread: thread attributes
  struct thread_dat tdat;       // data structure to communicate with thread
  tdat.blobnr = 0;
  tdat.bExit = 0;
  pthread_attr_init(&pt_attr);  // pthread: create and init thread attribute
  // TODO: create thread via pthread_create()
  // thread handle ... &cam_thread
  // thread attributes ... &pt_attr
  // thread routine ... worker
  // parameter shared with thread routine: &tdat
  pthread_create(&cam_thread, &pt_attr, worker, &tdat);

  camcar(argc, argv, &tdat);    // start control loop in main thread

  tdat.bExit = 1;               // signal thread to terminate
  // TODO: wait for thread to finish via pthread_join():
  //       thread handle ... cam_thread
  //       pointer return value ... NULL
  
  pthread_join(cam_thread, NULL);

  // TODO: distroy thread attribute pt_attr via pthread_attr_destroy()
  //       thread attributes: &pt_attr
  
  pthread_attr_destroy(&pt_attr);

  initio_Cleanup ();  // initio: cleanup the library (reset robot car)
  endwin();           // curses: cleanup the library
  return EXIT_SUCCESS;
}

