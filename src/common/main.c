/* main.c - Frotz V2.40 main function
 *	Copyright (c) 1995-1997 Stefan Jokisch
 *
 * This file is part of Frotz.
 *
 * Frotz is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Frotz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 * This is an interpreter for Infocom V1 to V6 games. It also supports
 * the recently defined V7 and V8 games. Please report bugs to
 *
 *    s.jokisch@avu.de
 *
 */

#include "frotz.h"
#define NU_SUCCESS 0

nio_console console;
nio_console status_line;

#ifndef MSDOS_16BIT
#define cdecl
#endif

extern void interpret (void);
extern void init_memory (void);
extern void init_undo (void);
extern void reset_memory (void);

/* Story file name, id number and size */

char *story_name = 0;

enum story story_id = UNKNOWN;
long story_size = 0;

/* Story file header data */

zbyte h_version = 0;
zbyte h_config = 0;
zword h_release = 0;
zword h_resident_size = 0;
zword h_start_pc = 0;
zword h_dictionary = 0;
zword h_objects = 0;
zword h_globals = 0;
zword h_dynamic_size = 0;
zword h_flags = 0;
zbyte h_serial[6] = { 0, 0, 0, 0, 0, 0 };
zword h_abbreviations = 0;
zword h_file_size = 0;
zword h_checksum = 0;
zbyte h_interpreter_number = 0;
zbyte h_interpreter_version = 0;
zbyte h_screen_rows = 0;
zbyte h_screen_cols = 0;
zword h_screen_width = 0;
zword h_screen_height = 0;
zbyte h_font_height = 1;
zbyte h_font_width = 1;
zword h_functions_offset = 0;
zword h_strings_offset = 0;
zbyte h_default_background = 0;
zbyte h_default_foreground = 0;
zword h_terminating_keys = 0;
zword h_line_width = 0;
zbyte h_standard_high = 1;
zbyte h_standard_low = 0;
zword h_alphabet = 0;
zword h_extension_table = 0;
zbyte h_user_name[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

zword hx_table_size = 0;
zword hx_mouse_x = 0;
zword hx_mouse_y = 0;
zword hx_unicode_table = 0;

/* Stack data */

zword stack[STACK_SIZE];
zword *sp = 0;
zword *fp = 0;
zword frame_count = 0;

/* IO streams */

bool ostream_screen = TRUE;
bool ostream_script = FALSE;
bool ostream_memory = FALSE;
bool ostream_record = FALSE;
bool istream_replay = FALSE;
bool message = FALSE;

/* Current window and mouse data */

int cwin = 0;
int mwin = 0;

int mouse_y = 0;
int mouse_x = 0;

/* Window attributes */

bool enable_wrapping = TRUE;
bool enable_scripting = FALSE;
bool enable_scrolling = FALSE;
bool enable_buffering = FALSE;

/* User options */

/*
int option_attribute_assignment = 0;
int option_attribute_testing = 0;
int option_context_lines = 0;
int option_object_locating = 0;
int option_object_movement = 0;
int option_left_margin = 0;
int option_right_margin = 0;
int option_ignore_errors = 0;
int option_piracy = 0;
int option_undo_slots = MAX_UNDO_SLOTS;
int option_expand_abbreviations = 0;
int option_script_cols = 80;
int option_save_quetzal = 1;
*/

int option_sound = 1;
char *option_zcode_path;


/* Size of memory to reserve (in bytes) */

long reserve_mem = 0;

/*
 * z_piracy, branch if the story file is a legal copy.
 *
 *	no zargs used
 *
 */

void z_piracy (void)
{
	
    branch (!f_setup.piracy);

}/* z_piracy */

#define NDLESS_CFG "/documents/ndless/ndless.cfg.tns"
#define BUF_SIZE 256

char *_fgets(char *s, int n, FILE *stream) {
    int done = 0;
    int i;
    if (s == NULL || n <= 0 || stream == NULL)
        return NULL;
    for (i = 0; !done && i < n - 1; ++i) {
        int c = fgetc(stream);
        if (c == EOF) {
            done = 1;
            --i;
        } else {
            s[i] = c;
            if (c == '\n')
                done = 1;
        }
    }
    s[i] = '\0';
    return (i == 0) ? NULL : s;
}

//nFrotz - strips a filename or path of the file's extension
//str is the path, ext_str is where to write the extension
char *get_ext(char *str,char *ext_str){
	int ext_str_pos = 0;
	
	ext_str[0] = 0;
	
	while(*str != '.' && *str != 0){
		str++;
	}
	
	while(*str != 0){
		ext_str[ext_str_pos] = *str;
		ext_str_pos++;
		str++;
	}
	
	ext_str[ext_str_pos] = 0;
	
	return ext_str;
}
//nFrotz - strips a filename or path of the file's name (without the extension)
//str is the path, file_str is where to write the name
char *get_file_name(char *str,char *file_str){
	int file_str_pos = 0;

	int pos = strlen(str)-1;
	
	while(pos != -1 && str[pos] != '\\' && str[pos] !='/'){
		pos--;
	}
	
	pos++;
	
	while(str[pos] != '.' && str[pos] != 0){
		file_str[file_str_pos++] = str[pos];
		pos++;
	}
	
	file_str[file_str_pos] = 0;
	
	return file_str;
}


//nFrotz - searches a directory and its subdirectories for all files that have a certain extension
//the output (file paths) is written to an array of c strings (256 bytes each)
//path is the initial path, ext is the extension to search for, list is the output array,
//and count is a pointer to an int that holds the number of files found (the int should
//be initialized to 0)
int find_all_files(char *path,char *ext,char *list,int *count){
	char newpath[256] = "A:";
	strcat(newpath,path);
	
	if(*path != 0){
		strcat(newpath,"\\");
	}
	
	strcat(newpath,"*.*");
	struct dstat statobj;
	int success;
	int write_pos = strlen(newpath)-3;
	
	char tempstr[128];
	
	if(NU_Get_First(&statobj,newpath) != NU_SUCCESS){
		return 0;
	}
	
	newpath[1] = '\\';
	
	do{
		char *fext = get_ext(statobj.filepath,tempstr);
		if(*fext == 0){
			if(find_all_files(statobj.filepath,ext,list,count) == 0){
				//return 0;
			}
		}
		else{
			if(strcmp(fext,ext) == 0){
				strcpy(newpath+write_pos,statobj.filepath);
				strcpy(list+count[0]*256,"/documents");
				strcat(list+count[0]*256,newpath+1);
				count[0]++;
			}
			
			
		}
		
		success = NU_Get_Next(&statobj);
	} while(success == NU_SUCCESS);
	
	NU_Done(&statobj);
	return 1;
}

char *file_list;				//nFrotz - list of files for the file browser

extern unsigned char *dbuf;		//nFrotz - double buffer for screen effects

//nFrotz - copies the double buffer to the screen
//#define CPS() memcpy(SCREEN_BASE_ADDRESS,dbuf,320*240/2)

//nFrotz - the name of the current file being run
char current_file_name[128];

//nFrotz - shows the game file browser
//file_name is where to store the path of the game that is selected
//returns true if a game was selected, or 0 is escape was pressed
bool show_file_browser(char **file_name){
	file_list = malloc(4096);
	
	nio_fprintf(&status_line,"\n  Select file");
	
	int count = 0;
	
	find_all_files("",".z.tns",file_list,&count);
	
	if(count == 0){
		nio_fprintf(&console,"<No game files found>");
		//CPS();
		while(!isKeyPressed(KEY_NSPIRE_ESC)) idle();
		return 0;
	}
	
	int i;
	char tempname[128];
	
	for(i = 0;i < count;i++){
		nio_fprintf(&console," %s\n",get_file_name(file_list+i*256,tempname));
	}
	
	int pos = 0;
	
	do{
		//console.cursor_x = 0;
		//console.cursor_y = pos;
		nio_fputc('>', &console);
		
		//CPS();
		wait_no_key_pressed();
		wait_key_pressed();
		
		//console.cursor_x = 0;
		//console.cursor_y = pos;
		nio_fputc(' ', &console);
	
		if(isKeyPressed(KEY_NSPIRE_UP)){
			if(pos == 0){
				pos = count-1;
			}
			else{
				pos--;
			}
		}
		
		if(isKeyPressed(KEY_NSPIRE_DOWN)){
			if(pos == count-1){
				pos = 0;
			}
			else{
				pos++;
			}
		}
		
		if(isKeyPressed(KEY_NSPIRE_ENTER)){
			break;
		}
		
		if(isKeyPressed(KEY_NSPIRE_ESC)){
			return 0;
		}
	} while(1);
	
	*file_name = pos*256+file_list;
	
	strcpy(current_file_name,get_file_name(*file_name,tempname));
	
	for(i = 0;i < 35;i++){
		nio_fprintf(&console,"\n");
		//CPS();
	}
	
	nio_clear(&console);
	//CPS();
	
	return 1;
	
}

//nFrotz - "installs" nFrotz, meaning it just adds the file extension association
//if the ndless revision is >= 538. Otherwise, it does nothing.
int install_nfrotz(void) {
	if(nl_ndless_rev() < 538){
		return 0;
	}
	
    /*if (show_msgbox_2b("Installer",
                       "nFrotz " NFROTZ_VERSION " (based on Frotz " VERSION ") by Christoffer Rehn.\n"
                       "You need a recent version of Ndless that supports file associations "
                       "for nFrotz to work.\nIf it doesn't, update Ndless and try again.\n"
                       "Do you want to install nFrotz?",
                       "Yes", "No") == 1) {}*/
	if(1){
        char buffer[BUF_SIZE] = {'\0'};
        FILE *fp;
        //assert_ndless_rev(538);
        fp = fopen(NDLESS_CFG, "a+");
        if (fp == NULL) {
            show_msgbox("Installer", "Couldn't access " NDLESS_CFG "!");
            return -1;
        }
        fseek(fp, 0, SEEK_SET);
        while (_fgets(buffer, BUF_SIZE, fp) != NULL)
            if (strcmp(buffer, "ext.z=nFrotz\n") == 0) {
                //show_msgbox("Installer",
				//          "nFrotz has already been installed."
				//        " You should be able to launch any Z-code files with a .z extension.");
                fclose(fp);
                return 0;
            }
        fprintf(fp, "ext.z=nFrotz\next.Z=nFrotz\n");
        fclose(fp);
        show_msgbox("nFrotz Installer", "nFrotz has been installed!\nLaunch games from My Documents or the builtin file browser");
		return 1;
    }
}

/*
 * main
 *
 * Prepare and run the game.
 *
 */

//nFrotz - current text and background colors of the console
char text_color;
char background_color;

void clear_status_line();
void cleanup_input();

void invert_screen(){
	//int y = 0;
	//int i;
	//unsigned char *screen = SCREEN_BASE_ADDRESS;
	
	//for(y = 0;y < 240;y++){
	//	for(i = 0;i < 320/2;i++){
	//		screen[(y * 320/2) + i] = ~screen[(y * 320/2) + i];
	//	}
	//}
}

//nFrotz - copy of the file browser before nFrotz is launched (for the
//scrolling effect)
void *save_screen;

//nFrotz - wipes the screen in the scrolling effect seen at the beginning
//color is whether or not the LCD is currently in color or gray
void wipe_screen(bool color){
	//save_screen = malloc(color ? 320*240*2 : 320*240/2);
	//memcpy(save_screen,SCREEN_BASE_ADDRESS,color ? 320*240*2 : 320*240/2);
	
	int i,d;
	unsigned short wipe_color;
	
	short byte_width = (color ? 320*2 : 320/2);
	
	for(i = 0;i < 240;i++){
		//memcpy(SCREEN_BASE_ADDRESS,SCREEN_BASE_ADDRESS+byte_width,240*byte_width-byte_width);
		
		if(i < 9){
			wipe_color = 0xFFFF;
		}
		else{
			wipe_color = 0;
		}
		
		for(d = 0;d < 320;d++){
			if(color){
		//		*((short *)SCREEN_BASE_ADDRESS + 239*320 + d) = wipe_color;
			}
			else{
		//		*((char *)SCREEN_BASE_ADDRESS + 239*byte_width + d) = wipe_color;
			}
		}
	
		//volatile int delay = color ? 0xFFFF/5 : (0xFFFF/5)*4;
			
		//for(;delay >= 0;delay--) ;
	}
}

//nFrotz - scrolls the screen back to My Documents
//color is whether or not the LCD is currently in color or gray
void restore_screen(bool color){
	int i,d;
	
	short byte_width = (color ? 320*2 : 320/2);
	
	//char *new_screen = malloc(byte_width*240*2);
	
	//memcpy(new_screen+240*byte_width,SCREEN_BASE_ADDRESS,byte_width*240);
	//memcpy(new_screen,save_screen,byte_width*240);
	
	for(i = 0;i < 239;i++){
	//	memcpy(SCREEN_BASE_ADDRESS,new_screen+(239-i)*byte_width,byte_width*240);
		
		volatile int delay = color ? 0xFFFF/5 : (0xFFFF/5)*4;
			
	//	for(;delay >= 0;delay--) ;
	}
	
	//free(save_screen);
	//free(new_screen);
}


//fast color macros
//#define getR(c) ((((((c) & 0xF800) >> 11) - 1) * 8) + 1)
//#define getG(c) ((((((c) & 0x7E0) >> 5) - 1) * 4) + 1)
//#define getB(c) (((((c) & 0x1F) - 1) * 8) + 1)
//#define getBW(c) ((((getR(c)) / 16) + ((getG(c)) / 16) + ((getB(c)) / 16)) / 3)

//nFrotz - converts the color contents of the screen to grayscale
void conv_screen_to_bw(){
	//unsigned char *bw_scr = malloc(320*240/2);
	//unsigned short *scr = SCREEN_BASE_ADDRESS;
	// int i;
	
	//for(i = 0;i < 320*240;i+=2){
	//	unsigned char bw_color1 = getBW((scr[i]));
	//	unsigned char bw_color2 = getBW((scr[i+1]));
	//	bw_scr[i/2] = (bw_color1 << 4) | bw_color2;
	//}
	
	volatile unsigned int *lcd_control = IO_LCD_CONTROL;
	
	//while(((*lcd_control >> 12) & 3) != 0) ;	//wait for vertical sync
	
	//lcd_ingray();
	//memcpy(SCREEN_BASE_ADDRESS,bw_scr,320*240/2);
	//free(bw_scr);
}

//nFrotz - converts the grayscale contents of the screen to color
void conv_screen_to_color(){
	// int i,d;
	// unsigned char *scr = SCREEN_BASE_ADDRESS;
	
	// for(i = 0;i < 240;i++){
	// 	for(d = 0;d < 320/2;d++){
	// 		scr[i*320/2 + d] = 0;
	// 	}
		
	// 	volatile int delay = 0xFFFF;
		
	// 	while(delay-- > 0) ;
	// }
	
	// volatile unsigned int *lcd_control = IO_LCD_CONTROL;
	// *lcd_control&=~(1 << 11);
	// lcd_incolor();
	// *lcd_control|=(1 << 11);
}
				
int cdecl main(int argc, char *argv[])
{
	bool force_bw = 0;	//for testing (forces the screen to grayscale mode)
	bool color = force_bw ? 0 : has_colors;
	
    bool ext_start = argc > 1;
    int new_argc = 6;
    char *new_argv[] = {
        argv[0],
        "-p",
        "-w 49",
        "-h 28",
        "-Z 2",
        ext_start ? argv[1] : NULL
    };

	//no file specified? Try installing nFrotz.
    if (!ext_start) {
        if(install_nfrotz() < 0){
			return 0;
		}
    }
	
	//for testing purposes, convert the screen to grayscale
	if(force_bw){
		conv_screen_to_bw();
	}
	
	//do the wiping effect
	wipe_screen(color);
	
	//now it's safe to convert the screen to grayscale
	if(color && !force_bw){
		conv_screen_to_bw();
	}
	
	//default console colors ('cause I like the traditional zork colors)
	text_color = WHITE;
	background_color = BLACK;	
	
	//create the consoles
    nio_init(&console, 54, 28, 1, 10, background_color, text_color, true);
	nio_clear(&console);
	nio_init(&status_line, 54, 1, 0, 0, text_color, background_color, true);
	nio_clear(&status_line);
	
	char *file_name;
		
	if(!ext_start){		//if no specified file, show the file browser
		if(!show_file_browser(&file_name)){
			goto done;	//spaghetti code!
		}
		
		new_argv[5] = file_name;
		printf("fiiiiiiile name = %s\n",file_name);
	}
	else{				//otherwise, set the current file name
		char name[128];
		strcpy(current_file_name,get_file_name(argv[1],name));
	}
	
	//Frotz startup stuff
    os_init_setup ();

    os_process_arguments (new_argc, new_argv);

    init_buffer ();

    init_err ();

    init_memory ();

    init_process ();

    init_sound ();
	
    os_init_screen ();
	//clear_status_line();

    init_undo ();

    z_restart ();
	
    interpret ();
	
	
done:
	if(color){	//convert the screen back to color
		conv_screen_to_color();
	}
	
	//restore the screen
	restore_screen(color);
	
	
	//if(has_colors){
	//	lcd_incolor();
	//}

    reset_memory ();

    os_reset_screen ();

    CLEANUP ();
	//cleanup_input();
	
	//free the list of files generated by the file browser
	if(file_list != NULL){
		free(file_list);
	}

    return 0;

}
