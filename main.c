/*
 * Names: Cole Plum & Mina Rulis
 * Date: 5.18.2020
 * File: main.c
 * Description: A game of battleship. The ships of sizes 2, 3, and 5 are randomly placed for each player. 
 * The server shoots first and the players alternate turns until there is a winner. A white space symbolizes a
 * missed shot, while a red space symbolized a hit ship.  
 */

#include "sockets.h"

int * missilePtr;
int sockfd;
int target_x = 0; 
int target_y = 0;
int white;
int red;
int green;
int blue;
int blank;
int myScreen[8][8];
int yourScreen[8][8];
char coord[10];
pi_framebuffer_t* fb;
pi_joystick_t* joystick;
char buffer[BUFFER_SIZE];

//Displays the array passed to it onto the framebuffer
void displayScreen(int screen[8][8]){
	clearBitmap(fb->bitmap, blank);
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 8; j++){
			setPixel(fb->bitmap, i, j, screen[i][j]);
		}
	}
}

//Quits the program and cleans up any memory when the user types CTRL+C
void interrupt_handler(int sig){
	close(sockfd);
	clearBitmap(fb->bitmap, blank);
	freeFrameBuffer(fb);
	freeJoystick(joystick);
	exit(0);
}

//Determines if a position will be in bounds of the framebuffer
bool inBounds(int pos){
	return (pos >= 0 && pos < 8);
}

//Randomly sets a piece of given length onto a given screen
void setPiece(int screen[8][8], int len){
	int xpos = rand() % 8;
	int ypos = rand() % 8;
	int dir = rand() % 3; 
	
	//Dir meanings
	//0 - Up
	//1 - Down
	//2 - Right
	//3 - Left
	
	int rows[8];
	int cols[8];

	if(dir <= 1){
	//Handles vertical ships
		if(dir == 0){ dir = -1;}
		for(int i = 0; i < len; i++){
			if(!(inBounds(xpos + (i*dir))) || (screen[xpos + (i*dir)][ypos] != 0)){
				setPiece(screen, len);
				return;			
			}
			else{
				//Adds to these arrays so that part of a ship is not placed when the space is invalid
				rows[i] = (xpos + (i*dir));
				cols[i] = ypos;
			}
		}
	}
	else if(dir >= 2){
	//Handles horizontal ships
		if(dir == 2){ dir = 1;}
		if(dir == 3){ dir = -1;}
		for(int i = 0; i < len; i++){
			if(!(inBounds(ypos + (i*dir))) || (screen[xpos][ypos + (i*dir)] != 0)){
				setPiece(screen, len);
				return;
			}
			else{
				//Adds to these arrays so that part of a ship is not placed when the space is invalid
				rows[i] = xpos;
				cols[i] = (ypos + (i*dir));
			}
		}
	}

	//Adds the entire ship once we know the entire ship can fit there
	for(int i = 0; i < len; i++){
		screen[rows[i]][cols[i]] = blue;
	}
}

bool playGame(){
	int countShips = 0;
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 8; j++){
			if(myScreen[i][j] == blue){
				countShips++;
			}	
		}
	}
	bzero(buffer, BUFFER_SIZE);
	if(!countShips){
		//You have no more ships afloat
		//Tell other player they won and quit
		strcpy(buffer, "You win");
		SendMsg(sockfd, buffer);
		printf("You lose...\n");
		interrupt_handler(2);	

	}
	else{
		//You still have ships afloat
		countShips = 0;
		for(int i = 0; i < 8; i++){
			for(int j = 0; j < 8; j++){
				if(yourScreen[i][j] == red){
					countShips++;
				}
			}
		}
		if(countShips == 10){
			//All ships have been hit
			strcpy(buffer, "You lose");
			SendMsg(sockfd, buffer);
			printf("You win!\n");
			interrupt_handler(2);
		}
		else{
			strcpy(buffer, "play");
			SendMsg(sockfd, buffer);
			return true;
		}
	}
}

//Called when the joystick is pressed while in the sendMissile(). Moves a target to somewhere on the screen and 
//sends a missile to the other player when pressed
void callbackFn(unsigned int code){
	int coord_num;
	setPixel(fb->bitmap, target_x, target_y, yourScreen[target_x][target_y]);
	switch(code){
		case KEY_UP:
			target_y=target_y==0?7:target_y-1;
			break;
		case KEY_DOWN: 
			target_y=target_y==7?0:target_y+1;
			break;
		case KEY_RIGHT:
			target_x=target_x==7?0:target_x+1;
			break;
		case KEY_LEFT:
			target_x=target_x==0?7:target_x-1;
			break;
		default:
		        coord_num = 10*target_x + target_y;
			sprintf(coord, "%d", coord_num);
			bzero(buffer, BUFFER_SIZE);
			strcpy(buffer, coord);
			SendMsg(sockfd, buffer);
			bzero(coord, 10);
			*missilePtr = 1;
	}	
	setPixel(fb->bitmap, target_x, target_y, green);
}

//Sends a missile to the other player. Once the joystick is pressed, the missile is send and the function exits
void sendMissile(){
	joystick = getJoystickDevice();
	int missileSent = 0;
	missilePtr = &missileSent;
	sleep(1);
	displayScreen(yourScreen);
	while(!(missileSent)){
		setPixel(fb->bitmap, target_x, target_y, green);
		pollJoystick(joystick, callbackFn, 1000);
	}
	usleep(500000);
}

//Receives the coordinates of an incoming missile in the form of a double-digit number and determines if the 
//missile hit a ship or not
void recvMissile(){
	sleep(1);
	displayScreen(myScreen);
	RecvMsg(sockfd, buffer);
	int coord_num;
        coord_num = atoi(buffer);
	target_x = coord_num / 10;
	target_y = coord_num % 10;
	if(myScreen[target_x][target_y] == blue || myScreen[target_x][target_y] == red){
		//A ship was hit
		myScreen[target_x][target_y] = red;
		strcpy(buffer, "hit");
		SendMsg(sockfd, buffer);
	}
	else{
		//A ship was not hit
		myScreen[target_x][target_y] = white;
		strcpy(buffer, "miss");
		SendMsg(sockfd, buffer);
	}
	displayScreen(myScreen);
}

//Receives a message which determines if the game is over or if it should keep going
void recvGameplayMsg(){
	RecvMsg(sockfd, buffer);
	if(strncmp(buffer, "play", 4) == 0){
		//Game continues
		return;
	}
	else if(strncmp(buffer, "You lose", 8) == 0){
		printf("You lose...\n");
		interrupt_handler(2);
	}
	else if(strncmp(buffer, "You win", 7) == 0){
		printf("You win!\n");
		interrupt_handler(2);
	}
}

//Receives a message telling you whether the missile you sent hit a ship or not. 
void recvIfHit(){
	RecvMsg(sockfd, buffer);
	if(strncmp(buffer, "hit", 3) == 0){
		yourScreen[target_x][target_y] = red;
		printf("You hit a ship!\n");
	}
	else if(strncmp(buffer, "miss", 4) == 0){
		yourScreen[target_x][target_y] = white;
		printf("You missed\n");
	}
	else{
		printf("Error receiving if hit...\nCalling again...\n");
		recvIfHit();
	}
	displayScreen(yourScreen);
}

//Main functin which sets up variables, takes care of server/client socket creation, and executes the gameplay loop
int main(int argc, char* argv[]){
	signal(SIGINT, interrupt_handler);
	white = getColor(255,255,255);
	red = getColor(255,0,0);
	green = getColor(0,255,0);
	blue = getColor(0,0,255);
	blank = getColor(0,0,0);
	fb = getFBDevice();

	//Sets screen arrays to be completely blank
	srand(time(0));
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 8; j++){
			myScreen[i][j] = blank;
			yourScreen[i][j] = blank;
		}
	}

	//Run server version before client version to connect properly
	if(argc == 2){
		//Server
		sockfd = CreateServer(argv);
		setPiece(myScreen, 2);
		setPiece(myScreen, 3);
		setPiece(myScreen, 5);
		displayScreen(myScreen);
		sleep(2);
		recvGameplayMsg();
		sendMissile();
		recvIfHit();
	}
	else if(argc == 3){
		//Client
		sockfd = CreateClient(argv);
		setPiece(myScreen, 2);
		setPiece(myScreen, 3);
		setPiece(myScreen, 5);
		displayScreen(myScreen);
	}
	else{
		fprintf(stderr, "\nInvalid Use...\nServer use: ./final <port>\nClient use: ./final <port> <server_name>\n\n");
		exit(0);
	}

	//Game loop
	while(playGame()){
		recvMissile();

		//recv a message ... either someone won or they didnt... other player is in playGame at this point and will send message from there
		//if message says game is over, end game. If not, keep going
		recvGameplayMsg();
		sendMissile();
		recvIfHit();
	}

}
