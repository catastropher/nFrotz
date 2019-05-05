
#include "frotz.h"

unsigned char *dbuf;	//the double buffer for showing screen effects

//copies the screen 
//#define CPS() memcpy(SCREEN_BASE_ADDRESS,dbuf,320*240/2)

typedef struct{		//holds a 2D vector (16x16 fixed point)
	int x,y;
} Vec2;

typedef struct{		//holds a particle from a particle system
	Vec2 pos;
	Vec2 speed;
} Particle;

typedef struct{		//structure for a particle system
	Particle *part;					//array of particles
	int total;						//number of particles currently in the array
	int max_part;					//max particles the array can hold
	unsigned char flags;			//flags affecting the rendering
	unsigned char part_color;		//color of the particles
	unsigned char back_color;		//color of the background
	
	int x1,y1;						//window region to draw into
	int x2,y2;
	
	int vis;						//number of visible particles in the system
	int steps;						//number of steps the system has taken since the simulation started
	
	int draw_delay;					//a delay to keep the system from going too fast
	
	int cx,cy;						//center of gravity well for attract/repel systems
} ParticleSystem;

enum{
	REVERSE_SYSTEM = 1,				//"rewind" the particle system
	INVERT_COLORS = 2,				//reverse the particle and background color
	NO_ERASE = 4,					//don't erase the particles during the simulation
	NO_GRAVITY = 8,					//disable gravity
	ATTRACT_CENTER = 16,			//pull particles to a point
	REPEL_CENTER = 32,				//accelerate the particles away from a point
	NO_DRAW = 64,					//don't draw the particles
	UPDATE_SMALL = 128				//for the first ~5 frames move the particles at 1/8 the speed
};

//NSpire I/O v2 was modified to allow us to specify where the console is draw to
extern unsigned char *draw_buf;

//creates a new particle system
bool create_particle_system(ParticleSystem *system,int max_particles,unsigned char flags){
	system->part = malloc(max_particles * sizeof(Particle));
	system->total = 0;
	system->max_part = max_particles;
	system->flags = flags;
	system->steps = 0;
	
	if(!system->part) return 0;
	return 1;
}

//frees the particles is the system
void cleanup_particle_system(ParticleSystem *system){
	free(system->part);
	system->part = NULL;
}

char getpix(short x,short y);

//examines the contents of dbuf and adds any pixels that are of pix_color to the particle system
void capture_particles(ParticleSystem *system,unsigned char pix_color,int x1,int y1,int x2,int y2){
	int x,y,k;
	int start = system->total;
	
	for(y = y1;y <= y2;y++){
		for(x = x1;x <= x2;x++){
			if(getpix(x,y) == pix_color && system->total < system->max_part){
				system->part[system->total].pos.x = x*65536;
				system->part[system->total].pos.y = y*65536;
				system->total++;
			}
		}
	}
	
	for(k = start;k < system->total;k++){
		//part[k].x = (rand()%320)*65536;
		//part[k].y = (rand()%240)*65536;
		
		do{
			system->part[k].speed.x = (rand()%1024*2*1)*256-2*65536*2;
			system->part[k].speed.y = (rand()%1024*2)*256-6*65536;
		} while(abs(system->part[k].speed.x) < 65536/4 || abs(system->part[k].speed.y) < 65536/2);
	}
}

//updates the particle system and counts the number of visible particles
void update_particle_system(ParticleSystem *system){
	int i;
	bool reverse_system = (system->flags & REVERSE_SYSTEM) != 0;
	
	system->vis = 0;
	
	for(i = 0;i < system->total;i++){
		bool visible = particle_visible(system,i);
		
		if(reverse_system){
			if((system->flags & NO_GRAVITY) == 0){
				if((system->flags & UPDATE_SMALL) == 0){
					system->part[i].speed.y-=32768/2;
				}
				else{
					system->part[i].speed.y-=32768/16;
				}
			}
			
			if((system->flags & UPDATE_SMALL) == 0){
				system->part[i].pos.x-=system->part[i].speed.x;
				system->part[i].pos.y-=system->part[i].speed.y;
			}
			else{
				system->part[i].pos.x-=system->part[i].speed.x/8;
				system->part[i].pos.y-=system->part[i].speed.y/8;
			}
		}
		else{
			if(visible){
				system->vis++;
			}
			
			if((system->flags & UPDATE_SMALL) == 0){
				system->part[i].pos.x+=system->part[i].speed.x;
				system->part[i].pos.y+=system->part[i].speed.y;
			}
			else{
				system->part[i].pos.x+=system->part[i].speed.x/8;
				system->part[i].pos.y+=system->part[i].speed.y/8;
			}
			
			if((system->flags & NO_GRAVITY) == 0){
				if((system->flags & UPDATE_SMALL) == 0){
					system->part[i].speed.y+=32768/2;
				}
				else{
					system->part[i].speed.y+=32768/16;
				}
			}
		}
	}
}

//sets a pixel to the given color
void setpix(short x,short y,unsigned char color){
	unsigned char *byte = dbuf+(y*320/2)+x/2;
	
	if((x&1) == 0){
		*byte = (*byte & 0x0F)|(color << 4);
	}
	else{
		*byte = (*byte & 0xF0)|(color);
	}
}

//returns the pixel color at the given position
char getpix(short x,short y){
	unsigned char *byte = dbuf+(y*320/2)+x/2;
	
	if((x & 1) == 0){
		return *byte >> 4;
	}
	else{
		return *byte & 0x0F;
	}
}

//returns whether the particle (with the index given by part) is
//visible or not
bool particle_visible(ParticleSystem *system,int part){
	int x = system->part[part].pos.x>>16;
	int y = system->part[part].pos.y>>16;
	
	if(y >= system->y1 && y <= system->y2){
		if(x >= system->x1 && x <= system->x2){
			return 1;
		}
	}
	
	return 0;
}

//draws all of the particles in the particle system in the given color
void draw_particle_system(ParticleSystem *system,unsigned char color){
	int i;
	
	for(i = 0;i < system->total;i++){
		if(particle_visible(system,i)){
			setpix(system->part[i].pos.x>>16,system->part[i].pos.y>>16,color);
		}
	}
}

//returns the sign of an int
inline char sign(int value){
	return value >= 0 ? 1 : -1;
}

//continuously updates and draws the particle system until the simulation is over,
//which is determined by what the flags are
int handle_particle_system(ParticleSystem *system){
	int old_steps = system->steps;
	bool reverse_system = (system->flags & REVERSE_SYSTEM) != 0;
	bool attract_center = (system->flags & ATTRACT_CENTER) != 0;
	bool repel_center = (system->flags & REPEL_CENTER) != 0;
	bool no_draw = (system->flags & NO_DRAW) != 0;
	bool update_small = (system->flags & UPDATE_SMALL) != 0;
	unsigned char back_color = system->back_color;
	unsigned char part_color = system->part_color;
	int i;
	int initial_attract_force = 60;
	int attract_force;
	int repel_force = 0xFFFFFFF;
	int x = system->cx << 16;
	int y = system->cy << 16;
	int update_small_steps = 5;
	
	if(repel_center){
		update_particle_system(system);
	}
	
	if((system->flags & INVERT_COLORS) != 0){
		unsigned char swap = back_color;
		back_color = part_color;
		part_color = swap;
	}
	
	system->steps = 0;
	
	draw_particle_system(system,back_color);
	
	do{
		system->steps++;
		attract_force = initial_attract_force+system->steps;
		
		if(update_small && !reverse_system && system->steps < update_small_steps){
			system->flags|=UPDATE_SMALL;
		}
		else{
			if(update_small){
				if(!reverse_system){
					system->flags&=~UPDATE_SMALL;
				}
				else{
					if(system->steps > old_steps+1-update_small_steps){
						system->flags|=UPDATE_SMALL;
					}
					else{
						system->flags&=~UPDATE_SMALL;
					}
				}
			}
		}
		
		if(attract_center || repel_center){
			int not_at_center = 0;
			system->vis = 0;
			for(i = 0;i < system->total;i++){
				int dist_x = x - system->part[i].pos.x;
				int dist_y = y - system->part[i].pos.y;
				
				if(particle_visible(system,i)){
					system->vis++;
				}
				else{
					continue;
				}
				
				if(attract_center && abs(dist_x) <= 0xFFFF && abs(dist_y) <= 0xFFFF){
					system->part[i].pos.x = x;
					system->part[i].pos.y = y;
					continue;
				}
				
				not_at_center++;
				
				int dx = dist_x>>16;
				int dy = dist_y>>16;
				
				int dist = dx*dx+dy*dy;
		
				if(attract_center){
					system->part[i].speed.x = (dist_x/dist)*attract_force;
					system->part[i].speed.y = (dist_y/dist)*attract_force;
					system->part[i].pos.x+=system->part[i].speed.x;
					system->part[i].pos.y+=system->part[i].speed.y;
				}
				else{
					//system->part[i].speed.x = (dist * (dist_x/65536.0/sqrt(dist)))*65536;
					//system->part[i].speed.y = (dist * (dist_y/65536.0/sqrt(dist)))*65536;
					dist/=32;
					system->part[i].pos.x+=((system->part[i].speed.x*(dist))>>8)+2*(system->part[i].speed.x);//+=system->part[i].speed.x;
					system->part[i].pos.y+=((system->part[i].speed.y*(dist))>>8)+2*(system->part[i].speed.y);//+=system->part[i].speed.y;
					
				}
				
				
				if(attract_center){
					//if it changes the side of the particle we're on, it should go back to the point
					if(sign(dist_x) != sign(x - system->part[i].pos.x) || sign(dist_y) != sign(y - system->part[i].pos.y)){
						system->part[i].pos.x = x;
						system->part[i].pos.y = y;
					}
				}
			}
			
			if(!no_draw){
				draw_particle_system(system,part_color);
			}
			
			if(not_at_center == 0) break;
		}
		else if(reverse_system){
			update_particle_system(system);
			if(!no_draw){
				draw_particle_system(system,part_color);
			}
			
			if(system->steps == old_steps+1){
				break;
			}
		}
		else{
			update_particle_system(system);
			if(!no_draw){
				draw_particle_system(system,part_color);
			}
		}
		
		if(!no_draw){
			CPS();
		}
		
		volatile int wait = system->draw_delay;
		
		while(wait-- >= 0) ;
		
		if(((system->flags & NO_ERASE) == 0) && !no_draw){
			draw_particle_system(system,back_color);
		}
		
	} while(system->vis > 0 || reverse_system);
	
	if(update_small){
		system->flags|=UPDATE_SMALL;
	}
}

bool skip_credits = 0;		//whether to skip the credits

const int def_delay = 0x3FFFF;

//types centered text on the screen
//pressing escape causes the particles on the screen to fall and the credits to be skipped
void center_text(ParticleSystem *system,int *y,char *str,int mode,int delay){
	int i,d;
	
	if(skip_credits) return;
	
	for(i = 0;i < strlen(str);i++){
		putChar(320/2 - strlen(str)*6/2+i*6,*y,str[i],BLACK,WHITE);
		
		CPS();
		
		for(d = 0;d < 8;d++){
			if(isKeyPressed(KEY_NSPIRE_ESC)){
				skip_credits = 1;
				system->total = 0;
				capture_particles(system,15,0,10,319,239);
				int k;
				
				for(k = 0;k < system->total;k++){
					system->part[k].speed.y = system->part[k].speed.y/4;
					system->part[k].speed.x = system->part[k].speed.x/2;
				}
				
				handle_particle_system(system);
				return;
			}
			idle();
		}
	}
	
	volatile int wait = delay;
	
	while(wait-- > 0) ;
	
	*y+=8;
}
		

void invert_screen();
extern unsigned char text_color;

//shows the credits
void show_credit_screen(){
	ParticleSystem system;
	create_particle_system(&system,20000,0);
	system.x1 = 0;
	system.y1 = 10;
	system.x2 = 319;
	system.y2 = 239;
	
	system.draw_delay = def_delay+0xFFFF;
	system.part_color = WHITE;
	system.back_color = BLACK;
	
	int start_y = 80;
	int y = start_y;
	
	system.cx = 320/2;
	system.cy = 240/2;
	dbuf = malloc(320*240/2);
	//memcpy(dbuf,SCREEN_BASE_ADDRESS,320*240/2);
	draw_buf = dbuf;
	
	center_text(&system,&y,"nFrotz - ported by:",0,0x7FFFF);
	y+=16;
	center_text(&system,&y,"Christoffer Rehn (hoffa)",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"and",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"Michael Wilder (catastropher)",0,0x7FFFFF);
	
	if(!skip_credits){
		capture_particles(&system,15,0,10,319,239);
		handle_particle_system(&system);
	}
	
	y = start_y;
	center_text(&system,&y,"Special thanks to:",0,0x7FFFF);
	y+=16;
	center_text(&system,&y,"Olivier Armand, for ndless",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"and",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"Julian Mackeben, for Nspire I/O",0,0x7FFFFF);
	system.total = 0;
	
	if(!skip_credits){
		capture_particles(&system,15,0,10,319,239);
		handle_particle_system(&system);
	}
	
	y = start_y;
	center_text(&system,&y,"Frotz created by:",0,0x7FFFF);
	y+=16;
	center_text(&system,&y,"Stefan Jokisch",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"and",0,0x7FFFF);
	y+=8;
	center_text(&system,&y,"Alfresco Petrofsky",0,0x7FFFFF);
	system.total = 0;
	
	if(!skip_credits){
		capture_particles(&system,15,0,10,319,239);
		handle_particle_system(&system);
	}
	
	cleanup_particle_system(&system);
}

//shows the black hole effect when the game quits
void show_end_screen(){
	ParticleSystem system;
	create_particle_system(&system,20000,ATTRACT_CENTER|NO_GRAVITY);
	system.x1 = 0;
	system.y1 = 0;
	system.x2 = 319;
	system.y2 = 239;
	
	system.draw_delay = def_delay;
	system.part_color = WHITE;
	system.back_color = BLACK;
	
	int start_y = 80;
	int y = start_y;
	
	system.cx = 320/2;
	system.cy = 240/2;
	
	if(text_color == BLACK){
		invert_screen();
	}
	//memcpy(dbuf,SCREEN_BASE_ADDRESS,320*240/2);
	
	capture_particles(&system,15,0,0,319,239);
	handle_particle_system(&system);
	
	system.flags = NO_GRAVITY|REPEL_CENTER;
	
	int i;
	
	for(i = 0;i < system.total;i++){
		system.part[i].speed.x = (rand()%1024*2*2)*256-2*65536*4;
		system.part[i].speed.y = (rand()%1024*2*2)*256-2*65536*4;
		
		while((long long)system.part[i].speed.x*system.part[i].speed.x + (long long)system.part[i].speed.y*system.part[i].speed.y < 
			(long long)(65536/4)*(65536/4)){
		
			system.part[i].speed.x*=2;
			system.part[i].speed.y*=2;
			
		}
	}
	
	handle_particle_system(&system);
	cleanup_particle_system(&system);
	free(dbuf);
}

extern char current_file_name[128];

extern nio_console status_line;

//performs the self assembly of the 
void show_initial_reverse(){
	char save_title[64];
	memcpy(save_title,status_line.data,51);
	
	nio_printf(&status_line,"\n  %s",current_file_name);
	
	//only display the first 9 rows
	//memcpy(SCREEN_BASE_ADDRESS,dbuf,9*320/2);
	
	ParticleSystem system;
	create_particle_system(&system,20000,NO_DRAW|NO_GRAVITY|UPDATE_SMALL);
	system.x1 = 0;
	system.y1 = 10;
	system.x2 = 319;
	system.y2 = 239;
	
	system.draw_delay = 0;//0x5FFFF;
	system.part_color = WHITE;
	system.back_color = BLACK;
	capture_particles(&system,15,0,9,319,239);
	
	int i;
	
	for(i = 0;i < system.total;i++){
		do{
			system.part[i].speed.x = ((rand()%1024*2*2)*256-2*65536*4);
			system.part[i].speed.y = ((rand()%1024*2*2)*256-2*65536*4);
		} while(abs(system.part[i].speed.x) < 65536/2 || abs(system.part[i].speed.y) < 65536/2);
	}
	
	//memset(dbuf,0,320*240/2);
	handle_particle_system(&system);
	system.flags = REVERSE_SYSTEM|NO_GRAVITY|UPDATE_SMALL;
	system.draw_delay = def_delay;
	
	handle_particle_system(&system);
	cleanup_particle_system(&system);
	//draw_buf = SCREEN_BASE_ADDRESS;
	nio_printf(&status_line,"\n");
	memcpy(status_line.data,save_title,51);
	//nio_DrawConsole(&status_line);
	
	wait_no_key_pressed();
}

