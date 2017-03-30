#include <LPC17xx.h>
#include <RTL.h>
#include <stdio.h>
#include "GLCD.h"
#include "KBD.h"
#include <stdlib.h>


int colours [6] = {DarkGrey, Cyan, Yellow, Navy, Magenta, Maroon};
const unsigned char ledPosArray[8] = { 28, 29, 31, 2, 3, 4, 5, 6 };
unsigned int lives = 2;
uint32_t X_val_paddle = 96;
uint32_t Y_val_paddle = 5;
uint32_t Y_val_ball = 16;//Y_val_ball is center of ball
uint32_t X_val_ball;//X_val_ball is center of ball
uint32_t Y_val_ball2 = 16;//Y_val_ball2 is center of ball
uint32_t X_val_ball2;//X_val_ball2 is center of ball
int delta_x = 10, delta_y = 10;
int delta_x2 = 10, delta_y2 = 10;
unsigned int rows = 1;
int brickarr [40][2];
int bricks;
volatile int totalbricks;
unsigned short ball[1], ball_erase[100], paddle[240], paddle_erase[240], brick_erase[200];
unsigned short ball_radius = 5;
int ball_released = 0;
int ball_avail = 1;
int ball2_released = 0;
int ball2_alive = 0;
int ball2_avail = 0;
int substituted = 0;//so that ball 2 is not generated for a certain case
OS_SEM sem1;
OS_TID taskpaddle, taskball, taskball2;


//Led Initialization
void LEDInit( void ) 
{
	LPC_SC->PCONP     |= (1 << 15);            
	LPC_PINCON->PINSEL3 &= ~(0xCF00);
	LPC_PINCON->PINSEL4 &= (0xC00F);
	LPC_GPIO1->FIODIR |= 0xB0000000;           
	LPC_GPIO2->FIODIR |= 0x0000007C;           
}

// Turn on the LED in a position within 0..7
void turnOnLED( unsigned char led ) 
{
	unsigned int mask = (1 << ledPosArray[led]);
	if ( led < 3 ) 
		LPC_GPIO1->FIOSET |= mask;
	else 
		LPC_GPIO2->FIOSET |= mask;
}

// Turn off the LED in the position within 0..7
void turnOffLED( unsigned char led ) 
{
	unsigned int mask = (1 << ledPosArray[led]);
	if ( led < 3 ) 
		LPC_GPIO1->FIOCLR |= mask;
	else 
		LPC_GPIO2->FIOCLR |= mask;
}

//Initializing Joystick
void KBD_Init (void) 
{
 LPC_SC->PCONP |= (1 << 15); /* enable power to GPIO & IOCON */
/* P1.20, P1.23..26 is GPIO (Joystick) */
 LPC_PINCON->PINSEL3 &= ~((3<< 8)|(3<<14)|(3<<16)|(3<<18)|(3<<20));
/* P1.20, P1.23..26 is input */
 LPC_GPIO1->FIODIR &= ~((1<<20)|(1<<23)|(1<<24)|(1<<25)|(1<<26));
}

//Get Joystick Value(Part of get_button)
uint32_t KBD_get (void) 
{
 uint32_t kbd_val;
 kbd_val = (LPC_GPIO1->FIOPIN >> 20) & KBD_MASK;
 return (kbd_val);
}

//Get Joystick Value 
uint32_t get_button (void) 
{
 uint32_t val = 0;
 val = KBD_get(); /* read Joystick state */
 val = (~val & KBD_MASK); /* key pressed is read as a non '0' value*/
 return (val);
} 

//Initialize INT0
void INT0Init( void ) 
{
	LPC_PINCON->PINSEL4 &= ~(3<<20); 
	LPC_GPIO2->FIODIR   &= ~(1<<10); 
	LPC_GPIOINT->IO2IntEnF |= (1 << 10);
	NVIC_EnableIRQ( EINT3_IRQn );
}

// INT0 interrupt handler
void EINT3_IRQHandler( void ) {

	// Check whether the interrupt is called on the falling edge. GPIO Interrupt Status for Falling edge.
	if ( LPC_GPIOINT->IO2IntStatF && (0x01 << 10) ) {
		LPC_GPIOINT->IO2IntClr |= (1 << 10); // clear interrupt condition

	}
	if (ball_avail==1)//if int0 is pressed and ball is available, release ball
	{
	ball_released=1;
	}
	if (ball2_avail==1)//if int0 is pressed and ball2 is available, release ball2
	{
	ball2_released=1;
	}
}

//Initialize Bricks for each new level
void initialize_bricks (unsigned short rows)
{
uint32_t X_val_brick;
int i, j, n;
unsigned short brick[200];
totalbricks = rows*8;//total bricks on screen
bricks = totalbricks;//bricks left on screen
	for (i=0; i<rows; i++)//number of rows of bricks
	{
		X_val_brick = 25-(i%2)*10;
		for (n=0; n<8; n++)//8 bricks per row
		{
			for (j=0; j<200; j++)
			{
				brick[j] = colours[i];
			}	
			GLCD_Bitmap (280-(i*15), X_val_brick, 10, 20, (unsigned char*)brick);
			brickarr[i*8+n][0] = X_val_brick;
			brickarr[i*8+n][1] = 280-(i*15);
			//amount_bricks+=1;
			X_val_brick +=25;		
		}
	}
}

//Create Ball
void create_ball (unsigned short ball_radius, uint32_t Y_val_ball, uint32_t X_val_ball, unsigned short ball[]){
int i, j;
ball[1] = Black;
	for (i = -ball_radius; i<ball_radius; i++){
		for (j = -ball_radius; j<ball_radius; j++){
			if(i*i + j*j <= ball_radius*ball_radius)
			GLCD_Bitmap (i+Y_val_ball, j+X_val_ball, 1, 1, (unsigned char*)ball);	
		}
	}	
}

//Deflection of ball off Paddle
void deflection (){
	int diff = (X_val_ball) - (X_val_paddle+24);
	if (diff < -12)
	{
	delta_x = -6;
	delta_y = 10;
	}
	else if (diff >= -12 && diff <= -5)
	{
	delta_x = -3;
	delta_y = 10;
	}
	else if (diff >= -4 && diff <= 4)
	{
	delta_x = 0;
	delta_y = 10;
	}
	else if (diff >= 5 && diff <= 12)
	{
	delta_x = 3;
	delta_y = 10;
	}
	else if (diff > 12)
	{
	delta_x = 6;
	delta_y = 10;
	}		
}

//Deflection of ball2 off Paddle
void deflection2 (){
	int diff = (X_val_ball2) - (X_val_paddle+24);
	if (diff < -12)
	{
	delta_x2 = -6;
	delta_y2 = 10;
	}
	else if (diff >= -12 && diff <= -5)
	{
	delta_x2 = -3;
	delta_y2 = 10;
	}
	else if (diff >= -4 && diff <= 4)
	{
	delta_x2 = 0;
	delta_y2 = 10;
	}
	else if (diff >= 5 && diff <= 12)
	{
	delta_x2 = 3;
	delta_y2 = 10;
	}
	else if (diff > 12)
	{
	delta_x2 = 6;
	delta_y2 = 10;
	}		
}
	
//Task Paddle Control
__task void paddle_control()
{
unsigned short paddle_width = 48;
unsigned short paddle_height = 5;
int i;

while (1)
	{
		i = get_button();
		
		os_sem_wait(sem1, 0xffff);//moves paddle when ready 
		
		if (i == 0x08 && X_val_paddle>0)// moving left
		{
		GLCD_Bitmap (Y_val_paddle, X_val_paddle, paddle_height, paddle_width, (unsigned char*)paddle_erase);
		X_val_paddle-=6;
		GLCD_Bitmap (Y_val_paddle, X_val_paddle, paddle_height, paddle_width,(unsigned char*)paddle);
		}
		else if (i == 0x20 && X_val_paddle<192)//moving right
		{
		GLCD_Bitmap (Y_val_paddle, X_val_paddle, paddle_height, paddle_width, (unsigned char*)paddle_erase);
		X_val_paddle+=6;
		GLCD_Bitmap (Y_val_paddle, X_val_paddle, paddle_height, paddle_width,(unsigned char*)paddle);
		}
		
		os_sem_send(sem1);//
		
		os_dly_wait(1000);//time buffer
	}
}

//Task Ball Control 2
__task void ball2_control (){
int brickhit2, j;

while (ball2_released==0)//wait for user to press int0 to release ball2
{
	os_sem_wait(sem1, 0xffff);//erase and draw ball according to paddle position when ready(flashing)
	GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
	X_val_ball2 = X_val_paddle+24;
	create_ball(ball_radius, Y_val_ball2, X_val_ball2, ball);
	os_sem_send(sem1);

}
	
os_dly_wait(1000);//time buffer
turnOffLED(7);
ball2_avail = 0;
ball2_alive = 1;

while (1){
	
	os_sem_wait(sem1, 0xffff);//erase and draw ball when ready
	GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
	Y_val_ball2 = Y_val_ball2+delta_y2;
	X_val_ball2 = X_val_ball2+delta_x2;
	create_ball(ball_radius, Y_val_ball2, X_val_ball2, ball);
	os_sem_send(sem1);
	if ((X_val_ball2>=225 && delta_x2>0) || (X_val_ball2<=15 && delta_x2<0))//hits side of screen
		delta_x2 *=-1;
	
	if (Y_val_ball2>310 && delta_y2>0)//hits top of screen
		delta_y2 *=-1;
	
	if (Y_val_ball2<=16 && X_val_ball2+5>=X_val_paddle && X_val_ball2-5<=X_val_paddle+48 && delta_y2<0)//bouncing off paddle
	{
	deflection2();	
 	}
	
	if (Y_val_ball2<=10)//hits bottom of screen (dies)									
	{
		ball2_alive=0;
		
		os_sem_wait(sem1, 0xffff);//delete when ready
		GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
		os_sem_send(sem1);
		Y_val_ball2 = 16;
		delta_x2 = 10;
		delta_y2 = 10;
		os_tsk_delete_self();//
		
	}
	
	brickhit2=0;
	for(j = 0; j<totalbricks; j++)//for all the bricks on screen
	{
		if(X_val_ball2 +5 >= brickarr[j][0] && X_val_ball2-5 <= brickarr[j][0]+20 && Y_val_ball2-5 <= brickarr[j][1]+10 && Y_val_ball2+5 >=brickarr[j][1])// if ball touches brick
		{
			os_sem_wait(sem1, 0xffff);//erase brick when ready
			GLCD_Bitmap(brickarr[j][1], brickarr[j][0], 10, 20, (unsigned char*)brick_erase);
			create_ball(ball_radius, Y_val_ball2, X_val_ball2, ball);//recreate ball at same lcoation after brick area is erased
			os_sem_send(sem1);
			bricks--;
			brickarr[j][0] = -999;
			brickarr[j][1] = -999;
			if(brickhit2==0)//only bounce once in the case 2 bricks are contacted
			{
			brickhit2++;
			delta_y2*=-1;// ball bouncing off bricks
			}
		}
	}
	
	os_dly_wait(5000);//time buffer
}
}


//Task Ball Control
__task void ball_control (){
int brickhit, i;

while (ball_released==0)//wait for user to press int0 to release ball
{
	os_sem_wait(sem1, 0xffff);//erase and draw ball according to paddle position when ready(flashing)
	GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
	X_val_ball = X_val_paddle+24;
	create_ball(ball_radius, Y_val_ball, X_val_ball, ball);
	os_sem_send(sem1);
}
os_dly_wait(1000);//time buffer
ball_avail = 0;

while (1){
	
	if (bricks <= (5*rows) && ball2_alive==0 && ball2_avail==0 && ball2_released==0 && substituted==0)//condition to get ball 2
	{
		turnOnLED(7);
		ball2_avail = 1;
		os_sem_wait(sem1, 0xffff);//create task when ready
		taskball2=os_tsk_create(ball2_control,1);//create ball2 task of priority 1
		os_sem_send(sem1);
		
	}
	
	os_sem_wait(sem1, 0xffff);//erase and draw ball when ready
	GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
	Y_val_ball = Y_val_ball+delta_y;
	X_val_ball = X_val_ball+delta_x;
	create_ball(ball_radius, Y_val_ball, X_val_ball, ball);
	os_sem_send(sem1);
	
	if ((X_val_ball>=225 && delta_x>0) || (X_val_ball<=15 && delta_x<0))//hits side of screen
		delta_x *=-1;
	
	if (Y_val_ball>310 && delta_y>0)//hits top of screen
		delta_y *=-1;
	
	if (Y_val_ball<=16 && X_val_ball+5>=X_val_paddle && X_val_ball-5<=X_val_paddle+48 && delta_y<0)//bouncing off paddle
	{
	deflection();	
 	}
	
	if (Y_val_ball<=10)//hits bottom of screen (dies)									
	{
		os_sem_wait(sem1, 0xffff);//delete when ready
		GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
		os_sem_send(sem1);
		
		if (ball2_alive==0 && ball2_avail==0)//lose a life if ball 2 is also dead
		{
		lives--;
		turnOffLED(lives);
			if (lives == 0)//game over
			{
				while (1){
					GLCD_Clear(White);
					printf ("      Game Over");
					os_sem_wait(sem1, 0xffff);//delete task when ready
					os_tsk_delete(taskpaddle);
					os_tsk_delete(taskball2);
					os_sem_send(sem1);
					os_tsk_delete_self();//
				}
			}
			Y_val_ball = 16;
			delta_x = 10;
			delta_y = 10;
			ball_released = 0;
			ball_avail = 1;
				while (ball_released==0)//wait for user to press int0 to release ball
				{
					os_sem_wait(sem1, 0xffff);//erase and draw ball according to paddle position when ready(flashing)
					GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
					X_val_ball = X_val_paddle+24;
					create_ball(ball_radius, Y_val_ball, X_val_ball, ball);
					os_sem_send(sem1);
				}
			os_dly_wait(1000);//time buffer
			ball_avail = 0;
				
		}
		
		else if (ball2_alive==1 && ball2_avail==0 &&ball2_released==1)// in this case, ball 2 coordinates become ball coordinates and ball 2 task is deleted(ball2 becomes ball)
		{
		os_sem_wait(sem1, 0xffff);//delete when ready
		GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
		os_tsk_delete(taskball2);//
		os_sem_send(sem1);
		
			
		ball2_alive = 0;
		Y_val_ball = Y_val_ball2;
		X_val_ball = X_val_ball2;
		delta_y = delta_y2;
		delta_x = delta_x2;
		}
		
		else if (ball2_alive==0 && ball2_avail==1 && ball2_released==0)// in this case, ball 2 coordinates become ball coordinates and ball 2 task is deleted(ball2 becomes ball)
		{
		os_sem_wait(sem1, 0xffff);//delete when ready
		GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
		os_tsk_delete(taskball2);//
		os_sem_send(sem1);
		
		substituted=1;
		turnOffLED(7);
		ball2_avail = 0;
		Y_val_ball = Y_val_ball2;
		X_val_ball = X_val_ball2;
		delta_y = delta_y2;
		delta_x = delta_x2;
		ball_released = 0;
		ball_avail = 1;
		while (ball_released==0)//wait for user to press int0 to release ball
		{
			os_sem_wait(sem1, 0xffff);//erase and draw ball according to paddle position when ready(flashing)
			GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
			X_val_ball = X_val_paddle+24;
			create_ball(ball_radius, Y_val_ball, X_val_ball, ball);
			os_sem_send(sem1);
		}
		os_dly_wait(1000);//time buffer
		ball_avail = 0;
		}
		
		os_dly_wait(1000);//time buffer
	}
	
	brickhit=0;
	for(i = 0; i<totalbricks; i++)//for all the bricks on screen
	{
		if(X_val_ball +5 >= brickarr[i][0] && X_val_ball-5 <= brickarr[i][0]+20 && Y_val_ball-5 <= brickarr[i][1]+10 && Y_val_ball+5 >=brickarr[i][1])// if ball touches brick
		{
			os_sem_wait(sem1, 0xffff);//erase brick when ready
			GLCD_Bitmap(brickarr[i][1], brickarr[i][0], 10, 20, (unsigned char*)brick_erase);
			create_ball(ball_radius, Y_val_ball, X_val_ball, ball);//recreate ball at same lcoation after brick area is erased
			os_sem_send(sem1);

			bricks--;
			brickarr[i][0] = -999;
			brickarr[i][1] = -999;
			if(brickhit==0)//only bounce once in the case 2 bricks are contacted
			{
			brickhit++;
			delta_y*=-1;// ball bouncing off bricks
			}
			
		}
	}
	
	if (bricks == 0)
	{
		if (rows==5)
		{
			while (1){
				GLCD_Clear(White);
				printf ("      You WIN ");
				os_sem_wait(sem1, 0xffff);//delete task when ready
				os_tsk_delete(taskpaddle);
				os_tsk_delete(taskball2);
				os_sem_send(sem1);
				os_tsk_delete_self();//
			}
		}
		lives++;
		turnOnLED(lives-1);
		os_sem_wait(sem1, 0xffff);//erase ball when ready
		GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
		os_sem_send(sem1);
		
		if (ball2_alive==1 || ball2_avail==1)
		{
			os_sem_wait(sem1, 0xffff);//erase ball2 when ready
			GLCD_Bitmap(Y_val_ball2-5, X_val_ball2-5, 10, 10, (unsigned char*)ball_erase);
			os_tsk_delete(taskball2);
			os_sem_send(sem1);
		}
		os_dly_wait(100000);//time buffer
		rows++;
		
		os_sem_wait(sem1, 0xffff);
		initialize_bricks(rows);
		os_sem_send(sem1);
		
		Y_val_ball = 16;
		Y_val_ball2 = 16;
		delta_x = 10;
		delta_y = 10;
		delta_x2 = 10;
		delta_y2 = 10;
		substituted = 0;
		ball_avail = 1;
		ball_released = 0;
		ball2_alive=0;
		turnOffLED(7);
		ball2_avail=0;
		ball2_released=0;

		while (ball_released==0)//wait for user to press int0 to release ball
		{
			os_sem_wait(sem1, 0xffff);//erase and draw ball according to paddle position when ready(flashing)
			GLCD_Bitmap(Y_val_ball-5, X_val_ball-5, 10, 10, (unsigned char*)ball_erase);
			X_val_ball = X_val_paddle+24;
			create_ball(ball_radius, Y_val_ball, X_val_ball, ball);
			os_sem_send(sem1);

		}
		os_dly_wait(1000);//time buffer	
	}
	os_dly_wait(5000);//time buffer
}
}

//Base Task
__task void base_task(){
	int i;
	for (i = 0; i<240; i++)//initialize paddle to red, ball to black, (paddle erase, brick erase, and ball erase) to white
	{
		if(i<0)
		{
			ball[i] = Black;
		}
		
		if(i<100)
		{
			ball_erase[i] = White;
		}
		
		if(i<200)
		{
			brick_erase[i] = White;
		}
		
		paddle[i] = Red;
		paddle_erase[i] = White;
	}
	GLCD_Bitmap (Y_val_paddle, X_val_paddle, 5, 48, (unsigned char*)paddle); // create paddle
	os_sem_init(sem1, 1);//initialize semaphore with inital token 1(able to use right away)

	taskpaddle=os_tsk_create(paddle_control,1);//create paddle task of priority 1
	
	os_sem_wait(sem1, 0xffff);
	taskball=os_tsk_create(ball_control,1);//create ball task of priority 1
	os_sem_send(sem1);

	
	while (1)//crucial delay
		os_dly_wait(10000);
}

int main() {
	int i;
	SystemInit(); 
	LEDInit();
	for (i=0; i<lives; i++)
	{
		turnOnLED(i);
	}
	INT0Init();
  GLCD_Init();
  GLCD_Clear(White); 	
	initialize_bricks(rows);
	os_sys_init(base_task);//
}
