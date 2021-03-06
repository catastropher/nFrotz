/* dumb-input.c
 * $Id: dumb-input.c,v 1.1.1.1 2002/03/26 22:38:34 feedle Exp $
 * Copyright 1997,1998 Alpine Petrofsky <alpine@petrofsky.berkeley.ca.us>.
 * Any use permitted provided this notice stays intact.
 */

#include "dumb_frotz.h"

f_setup_t f_setup;
nio_console console;

static char runtime_usage[] =
    "DUMB-FROTZ runtime help:\n"
    "  General Commands:\n"
    "    \\help    Show this message.\n"
    "    \\set     Show the current values of runtime settings.\n"
    "    \\s       Show the current contents of the whole screen.\n"
    "    \\d       Discard the part of the input before the cursor.\n"
    "    \\wN      Advance clock N/10 seconds, possibly causing the current\n"
    "                and subsequent inputs to timeout.\n"
    "    \\w       Advance clock by the amount of real time since this input\n"
    "                started (times the current speed factor).\n"
    "    \\t       Advance clock just enough to timeout the current input\n"
    "  Reverse-Video Display Method Settings:\n"
    "    \\rn   none    \\rc   CAPS    \\rd   doublestrike    \\ru   underline\n"
    "    \\rbC  show rv blanks as char C (orthogonal to above modes)\n"
    "  Output Compression Settings:\n"
    "    \\cn      none: show whole screen before every input.\n"
    "    \\cm      max: show only lines that have new nonblank characters.\n"
    "    \\cs      spans: like max, but emit a blank line between each span of\n"
    "                screen lines shown.\n"
    "    \\chN     Hide top N lines (orthogonal to above modes).\n"
    "  Misc Settings:\n"
    "    \\sfX     Set speed factor to X.  (0 = never timeout automatically).\n"
    "    \\mp      Toggle use of MORE prompts\n"
    "    \\ln      Toggle display of line numbers.\n"
    "    \\lt      Toggle display of the line type identification chars.\n"
    "    \\vb      Toggle visual bell.\n"
    "    \\pb      Toggle display of picture outline boxes.\n"
    "    (Toggle commands can be followed by a 1 or 0 to set value ON or OFF.)\n"
    "  Character Escapes:\n"
    "    \\\\  backslash    \\#  backspace    \\[  escape    \\_  return\n"
    "    \\< \\> \\^ \\.  cursor motion        \\1 ..\\0  f1..f10\n"
    "    \\D ..\\X   Standard Frotz hotkeys.  Use \\H (help) to see the list.\n"
    "  Line Type Identification Characters:\n"
    "    Input lines:\n"
    "      untimed  timed\n"
    "      >        T      A regular line-oriented input\n"
    "      )        t      A single-character input\n"
    "      }        D      A line input with some input before the cursor.\n"
    "                         (Use \\d to discard it.)\n"
    "    Output lines:\n"
    "      ]     Output line that contains the cursor.\n"
    "      .     A blank line emitted as part of span compression.\n"
    "            (blank) Any other output line.\n"
;

static float speed = 1;
static bool do_more_prompts = FALSE;

enum input_type {
    INPUT_CHAR,
    INPUT_LINE,
    INPUT_LINE_CONTINUED,
};

extern nio_console status_line;
extern char text_color;
extern char background_color;
char adaptive_cursor[5][6] =
{
	{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, // block cursor
	{0xF7,0xF3,0x01,0x01,0xF3,0xF7}, // arrow cursor
	{0x83,0xED,0xEE,0xED,0x83,0xFF}, // 'A' cursor
	{0xDF,0xAB,0xAB,0xAB,0xC7,0xFF}, // 'a' cursor
	{0xDB,0x81,0xDB,0xDB,0x81,0xDB}  // '#' cursor
};
void invert_screen();

typedef struct
{
	char* data;
	unsigned short* color;
	int cursor_x;
	int cursor_y;
	int max_x;
	int max_y;
	int offset_x;
	int offset_y;
	unsigned char default_background_color;
	unsigned char default_foreground_color;
	BOOL drawing_enabled;
	BOOL cursor_enabled;
	int cursor_type;
	int cursor_line_width;
	unsigned char cursor_custom_data[6];
	BOOL cursor_blink_enabled;
	BOOL cursor_blink_status;
	unsigned cursor_blink_timestamp;
	unsigned cursor_blink_duration;
} nio_cursor;

unsigned int nio_cursor_clock(void) {
    return *(volatile unsigned*)0x90090000;
}

nio_cursor cursor = {NULL,NULL,0,0,0,0,0,0,0,0,1,1,4,1,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},1,0,0,0};

#define NIO_CHAR_WIDTH 6
#define NIO_CHAR_HEIGHT 8
#define nio_pixel_set setPixel
#define nio_csl_drawchar nio_DrawChar

void nio_cursor_draw(nio_cursor* c,nio_console *con)
{
	unsigned short color = text_color;//c->color[c->cursor_y*c->max_x+c->cursor_x];
	
	unsigned char foreground_color = color;
	
	// The starting position of where to draw the cursor
	const unsigned short cursor_x_start = con->offset_x + con->cursor_x*NIO_CHAR_WIDTH;
	const unsigned short cursor_y_start = con->offset_y + (con->cursor_y*NIO_CHAR_HEIGHT) + NIO_CHAR_HEIGHT;

	if((c->cursor_enabled)) {
		if (!((c->cursor_type >= 0) && (c->cursor_type <= 4))) {
			// Set the cursor type to a valid one
			c->cursor_type = 0;
		}
		
		int i, j;
		
		c->cursor_type = 0;
		
		if (c->cursor_type == 0) {
			// Draw a box for the cursor
			for(i = 0; i < NIO_CHAR_WIDTH; i++)
			{
				for(j = NIO_CHAR_HEIGHT; j > 0; j--)
				{
					nio_pixel_set(cursor_x_start+i,cursor_y_start-j, foreground_color);
				}
			}
		} else if (c->cursor_type == 1) {
			// Draw a horizontal underline (underscore) for the cursor
			
			// Sanity check for cursor_line_width
			if (!((c->cursor_line_width > 0) && (c->cursor_line_width <= NIO_CHAR_HEIGHT))) {
				// Set the cursor width to 1 (regular cursor width)
				c->cursor_line_width = 1;
			}
			
			// Draw it!
			for(i = 0; i < NIO_CHAR_WIDTH; i++)
			{
				//for(j = NIO_CHAR_HEIGHT; j > (NIO_CHAR_HEIGHT - c->cursor_line_width); j--)
				for(j = 0; j < c->cursor_line_width; j++)
				{
					nio_pixel_set(cursor_x_start+i,cursor_y_start-j-1, foreground_color);
				}
			}
		} else if (c->cursor_type == 2) {
			// Draw a vertical bar for the cursor
			
			// Sanity check for cursor_line_width
			if (!((c->cursor_line_width > 0) && (c->cursor_line_width <= NIO_CHAR_WIDTH))) {
				// Set the cursor width to 1 (regular cursor width)
				c->cursor_line_width = 1;
			}
			
			// Draw it!
			for(i = 0; i < c->cursor_line_width; i++) {
				for(j = NIO_CHAR_HEIGHT; j > 0; j--)
				{
					nio_pixel_set(cursor_x_start+i,cursor_y_start-j, foreground_color);
				}
			}
		} else if (c->cursor_type == 3 || c->cursor_type == 4) {
			// Draw a custom cursor
			// This uses pretty much the same code as the regular character drawing
			
			unsigned char b_color = color;
			
			// Sanity check to make sure the user defined something for the character
			if (c->cursor_custom_data == NULL) {
				// Set the cursor to a full cursor
				int p;
				for(p = 0; p <= 5; p++)
					c->cursor_custom_data[p] = 0xFF;
			}
			
			// Draw it!
			int pixelOn;
			for(i = 0; i < NIO_CHAR_WIDTH; i++)
			{
				for(j = NIO_CHAR_HEIGHT; j > 0; j--)
				{
					pixelOn = c->cursor_custom_data[i] << j;
					pixelOn = pixelOn & 0x80 ;
					if (pixelOn) 		nio_pixel_set(cursor_x_start+i,cursor_y_start-j,foreground_color);
					else if(!pixelOn) 	nio_pixel_set(cursor_x_start+i,cursor_y_start-j,background_color);
				}
			}
		}
	}
}

void nio_cursor_erase(nio_cursor* c,nio_console *con)
{
	//unsigned short color = background_color;//c->color[con->cursor_y*con->max_x+c->cursor_x];
	
	//char background_color = color;// >> 8;
	
	// The starting position of where to draw the cursor
	const unsigned short cursor_x_start = con->offset_x + con->cursor_x*NIO_CHAR_WIDTH;
	const unsigned short cursor_y_start = con->offset_y + (con->cursor_y*NIO_CHAR_HEIGHT) + NIO_CHAR_HEIGHT;
	
	// Draw a box for the cursor
	if((c->drawing_enabled) && (c->cursor_enabled)) {
		int i, j;
		for(i = 0; i < NIO_CHAR_WIDTH; i++)
		{
			for(j = NIO_CHAR_HEIGHT; j > 0; j--)
			{
				nio_pixel_set(cursor_x_start+i,cursor_y_start-j, background_color);
			}
		}
		//nio_csl_drawchar(con,con->cursor_x,con->cursor_y);
	}
}

void nio_cursor_blinking_draw(nio_cursor* c,nio_console *con)
{
	if (!c->cursor_blink_enabled) return;
	if (c->cursor_blink_timestamp == 0) c->cursor_blink_timestamp = nio_cursor_clock();
	if (c->cursor_blink_duration == 0) c->cursor_blink_duration = 1;
	
	if ((nio_cursor_clock() - c->cursor_blink_timestamp) >= c->cursor_blink_duration) {
		if (c->cursor_blink_status)
			nio_cursor_draw(c,con);
		else
			nio_cursor_erase(c,con);
		c->cursor_blink_status = !c->cursor_blink_status;
		c->cursor_blink_timestamp = nio_cursor_clock();
	}
}

void nio_cursor_blinking_reset(nio_cursor* c)
{
	if (!c->cursor_blink_enabled) return;
	c->cursor_blink_timestamp = nio_cursor_clock();
}

void nio_cursor_enable(nio_cursor* c, BOOL enable_cursor,nio_console *con)
{
	c->cursor_enabled = enable_cursor;
	if (!enable_cursor)
		nio_cursor_erase(c,con);
	else
		nio_cursor_draw(c,con);
}

void nio_cursor_blinking_enable(nio_cursor* c, BOOL enable_cursor_blink,nio_console *con)
{
	c->cursor_blink_enabled = enable_cursor_blink;
	if (!enable_cursor_blink) nio_cursor_draw(c,con);
}

void nio_cursor_blinking_duration(nio_cursor* c, int duration)
{
	c->cursor_blink_duration = (unsigned) duration;
}

void nio_cursor_type(nio_cursor* c, int cursor_type)
{
	if (!((c->cursor_type >= 0) && (c->cursor_type <= 4))) {
		// Set the cursor type to a valid one
		cursor_type = 0;
	}
	c->cursor_type = cursor_type;
}

void nio_cursor_width(nio_cursor* c, int cursor_width)
{
	c->cursor_line_width = cursor_width;
}

void nio_cursor_custom(nio_cursor* c, unsigned char cursor_data[6])
{
	int i;
	for(i = 0; i <= 5; i++) {
		c->cursor_custom_data[i] = cursor_data[i];
	}
	//memcpy(c->cursor_custom_data, cursor_data, sizeof(cursor_data));
}


extern BOOL shift;
extern BOOL caps;
extern BOOL ctrl;
static char shiftKey(const char normalc, const char shiftc)
{
	if(shift || caps) 
	{
		shift = FALSE;
		return shiftc;
	}
	else return normalc;
}
static char shiftOrCtrlKey(const char normalc, const char shiftc, const char ctrlc)
{
	if(shift || caps)
	{
		shift = FALSE;
		return shiftc;
	}
	else if(ctrl)
	{
		ctrl = FALSE;
		return ctrlc;
	}
	else return normalc;
}

char nio_ascii_get(int* adaptive_cursor_state)
{
	// Ctrl, Shift, Caps first
	if(isKeyPressed(KEY_NSPIRE_CTRL))
	{
		if(ctrl)
		{
			ctrl = FALSE;
			*adaptive_cursor_state = 0;
		}
		else
		{
			ctrl = TRUE;
			*adaptive_cursor_state = 4;
		}
		return 1; // Indicates that no key has been pressed - the cursor will continue flashing.
	}
	if(isKeyPressed(KEY_NSPIRE_SHIFT))
	{
		if(ctrl)
		{
			ctrl = FALSE;
			shift = FALSE;
			caps = TRUE;
			*adaptive_cursor_state = 2;
		}
		else if(caps)
		{
			caps = FALSE;
			*adaptive_cursor_state = 0;
		}
		else if(shift)
		{
			shift = FALSE;
			*adaptive_cursor_state = 0;
		}
		else
		{
			shift = TRUE;
			*adaptive_cursor_state = 1;
		}
		return 1;
	}
	
	if(caps)
		*adaptive_cursor_state = 2;
	
	if(isKeyPressed(KEY_NSPIRE_ESC)) return 0;
	
	// Characters
	if(isKeyPressed(KEY_NSPIRE_A)) return shiftKey('a','A');
	if(isKeyPressed(KEY_NSPIRE_B)) return shiftKey('b','B');
	if(isKeyPressed(KEY_NSPIRE_C)) return shiftKey('c','C');
	if(isKeyPressed(KEY_NSPIRE_D)) return shiftKey('d','D');
	if(isKeyPressed(KEY_NSPIRE_E)) return shiftKey('e','E');
	if(isKeyPressed(KEY_NSPIRE_F)) return shiftKey('f','F');
	if(isKeyPressed(KEY_NSPIRE_G)) return shiftKey('g','G');
	if(isKeyPressed(KEY_NSPIRE_H)) return shiftKey('h','H');
	if(isKeyPressed(KEY_NSPIRE_I)) return shiftKey('i','I');
	if(isKeyPressed(KEY_NSPIRE_J)) return shiftKey('j','J');
	if(isKeyPressed(KEY_NSPIRE_K)) return shiftKey('k','K');
	if(isKeyPressed(KEY_NSPIRE_L)) return shiftKey('l','L');
	if(isKeyPressed(KEY_NSPIRE_M)) return shiftKey('m','M');
	if(isKeyPressed(KEY_NSPIRE_N)) return shiftKey('n','N');
	if(isKeyPressed(KEY_NSPIRE_O)) return shiftKey('o','O');
	if(isKeyPressed(KEY_NSPIRE_P)) return shiftKey('p','P');
	if(isKeyPressed(KEY_NSPIRE_Q)) return shiftKey('q','Q');
	if(isKeyPressed(KEY_NSPIRE_R)) return shiftKey('r','R');
	if(isKeyPressed(KEY_NSPIRE_S)) return shiftKey('s','S');
	if(isKeyPressed(KEY_NSPIRE_T)) return shiftKey('t','T');
	if(isKeyPressed(KEY_NSPIRE_U)) return shiftKey('u','U');
	if(isKeyPressed(KEY_NSPIRE_V)) return shiftKey('v','V');
	if(isKeyPressed(KEY_NSPIRE_W)) return shiftKey('w','W');
	if(isKeyPressed(KEY_NSPIRE_X)) return shiftKey('x','X');
	if(isKeyPressed(KEY_NSPIRE_Y)) return shiftKey('y','Y');
	if(isKeyPressed(KEY_NSPIRE_Z)) return shiftKey('z','Z');
	
	// Numbers
	if(isKeyPressed(KEY_NSPIRE_0)) return '0';
	if(isKeyPressed(KEY_NSPIRE_1)) return '1';
	if(isKeyPressed(KEY_NSPIRE_2)) return '2';
	if(isKeyPressed(KEY_NSPIRE_3)) return '3';
	if(isKeyPressed(KEY_NSPIRE_4)) return '4';
	if(isKeyPressed(KEY_NSPIRE_5)) return '5';
	if(isKeyPressed(KEY_NSPIRE_6)) return '6';
	if(isKeyPressed(KEY_NSPIRE_7)) return '7';
	if(isKeyPressed(KEY_NSPIRE_8)) return '8';
	if(isKeyPressed(KEY_NSPIRE_9)) return '9';
	
	// Symbols
	if(isKeyPressed(KEY_NSPIRE_COMMA))		return shiftKey(',',';');
	if(isKeyPressed(KEY_NSPIRE_PERIOD)) 	return shiftKey('.',':');
	if(isKeyPressed(KEY_NSPIRE_COLON))		return ':';
	if(isKeyPressed(KEY_NSPIRE_LP))			return shiftOrCtrlKey('(','[',']');
	if(isKeyPressed(KEY_NSPIRE_RP))			return shiftOrCtrlKey(')','{','}');
	if(isKeyPressed(KEY_NSPIRE_SPACE))		return shiftKey(' ','_');
	if(isKeyPressed(KEY_NSPIRE_DIVIDE))		return shiftKey('/','\\');
	if(isKeyPressed(KEY_NSPIRE_MULTIPLY))	return shiftKey('*','\"');
	if(isKeyPressed(KEY_NSPIRE_MINUS))		return shiftKey('-','_');
	if(isKeyPressed(KEY_NSPIRE_NEGATIVE))	return shiftKey('-','_');
	if(isKeyPressed(KEY_NSPIRE_PLUS))		return '+';
	if(isKeyPressed(KEY_NSPIRE_EQU))		return '=';
	if(isKeyPressed(KEY_NSPIRE_LTHAN))		return '<';
	if(isKeyPressed(KEY_NSPIRE_GTHAN))		return '>';
	if(isKeyPressed(KEY_NSPIRE_QUOTE))		return '\"';
	if(isKeyPressed(KEY_NSPIRE_APOSTROPHE))	return '\'';
	if(isKeyPressed(KEY_NSPIRE_QUES))		return shiftKey('?','!');
	if(isKeyPressed(KEY_NSPIRE_QUESEXCL))	return shiftKey('?','!');
	if(isKeyPressed(KEY_NSPIRE_BAR))		return '|';
	if(isKeyPressed(KEY_NSPIRE_EXP))		return '^';
	if(isKeyPressed(KEY_NSPIRE_ENTER))		return shiftKey('\n','~');
	if(isKeyPressed(KEY_NSPIRE_SQU))		return '�';
	
	// Special chars
	#ifdef KEY_NSPIRE_CLEAR // Keep better Ndless 2 compatibility (clickpad)
	if(isKeyPressed(KEY_NSPIRE_DEL)
	 ||isKeyPressed(KEY_NSPIRE_CLEAR))		return '\b';
	#else
	if(isKeyPressed(KEY_NSPIRE_DEL))		return '\b';
	#endif
	if(isKeyPressed(KEY_NSPIRE_RET))		return '\n';
	if(isKeyPressed(KEY_NSPIRE_TAB))		return '\t';
	
	return 0;
}

char *prev_input[11];
int prev_input_pos;
int current_input_pos;

void cleanup_input(){
	int i;
	for(i = 0;i < prev_input_pos;i++){
		free(prev_input[i]);
		prev_input[i] = 0;
	}
	
	prev_input_pos = 0;
}

int nio_getch_special(nio_console* c)
{
	while(1)
	{
		while (!any_key_pressed()){
            nio_cursor_blinking_draw(&cursor,c);
			//idle();
		}
		
		if(isKeyPressed(KEY_NSPIRE_TAB)){
			while(isKeyPressed(KEY_NSPIRE_TAB)) ;
			
			//nio_cursor_erase(&cursor,&console);
			char temp = text_color;
			text_color = background_color;
			background_color = temp;
			
			//printf("text_color = %d\n",text_color);			
			nio_SetColor(&console,background_color,text_color);
			nio_SetColor(&status_line,text_color,background_color);
			invert_screen();
			
			int i;
			for(i = 0;i < c->max_x*c->max_y;i++){
				c->color[i] = (background_color << 4) | text_color;
			}
			continue;
		}
		
		if(isKeyPressed(KEY_NSPIRE_UP)){
			while(isKeyPressed(KEY_NSPIRE_UP)) ;
			
			return 1;
		}
		
		if(isKeyPressed(KEY_NSPIRE_DOWN)){
			while(isKeyPressed(KEY_NSPIRE_DOWN)) ;
			
			return 2;
		}
		
		nio_cursor_blinking_reset(&cursor);
		cursor.cursor_blink_status = FALSE;
		//nio_cursor_draw(&cursor,c);
		
        //nio_cursor_erase(c);
		int adaptive_cursor_state = 0;
        char tmp = nio_ascii_get(&adaptive_cursor_state);
		
		if(cursor.cursor_type == 4)
		{
			nio_cursor_custom(&cursor,&adaptive_cursor[adaptive_cursor_state][0]);
		}
		if(tmp == 1 || tmp == 0) // To be compatible with the ANSI C++ _getch, pressing ESC does not abort this function (if 0 is returned from nio_ascii_get)
		{						// 1 is just a special case for modifier keys
			wait_no_key_pressed();
			continue;
		}
		else return tmp;
	}
}

extern int quit_status;

char* nio_fgets_special(char* str, int num, nio_console* c)
{
	char tmp;
	int old_x = c->cursor_x;
	int old_y = c->cursor_y;
	
	nio_cursor_blinking_reset(&cursor);
	cursor.cursor_blink_status = TRUE;
	//nio_cursor_draw(&cursor,c);
	
	
	int i = 0;
	while(i < num-2)
	{
        while(any_key_pressed()){
			nio_cursor_blinking_reset(&cursor);
		}
		tmp = nio_getch_special(c);
		
		if(tmp == 1 || tmp == 2){
			nio_cursor_erase(&cursor,c);
			if((current_input_pos == 0 && tmp == 1) || (current_input_pos == prev_input_pos && tmp == 2)){
				continue;
			}
			
			//erase the old string
			int d;
			for(d = 0;d < strlen(str);d++){
				nio_printf(c,"\b \b");
				nio_cursor_erase(&cursor,c);
			}
			
			//save the current line
			if(current_input_pos == prev_input_pos){
				prev_input[prev_input_pos] = malloc(1000);
				strcpy(prev_input[prev_input_pos],str);
			}
			
			if(tmp == 1){
				current_input_pos--;
			}
			else if(tmp == 2){
				current_input_pos++;
			}
			
			//remove any '\n'
			int s_pos = 0;
			int d_pos = 0;
			char *n_str = prev_input[current_input_pos];
			
			for(s_pos = 0;s_pos < strlen(n_str)+1;s_pos++){
				if(n_str[s_pos] != '\n'){
					n_str[d_pos] = n_str[s_pos];
					d_pos++;
				}
			}
			
			//copy the saved string to the edit string
			strcpy(str,n_str);
			
			//if the new position is the one we're editing, we can free the saved string
			if(current_input_pos == prev_input_pos){
				free(prev_input[current_input_pos]);
				prev_input[current_input_pos] = 0;
			}
			
			//print the new string
			for(d = 0;d < strlen(str);d++){
				nio_PrintChar(c,str[d]);
			}
			i = d;
			
			if(cursor.cursor_blink_status == FALSE){
				nio_cursor_draw(&cursor,c);
			}
		}
			
		else if(tmp == '\n')
		{
			str[i] = '\0';
			nio_cursor_erase(&cursor,c);
			nio_PrintChar(c,'\n');
			return i > 0 ? str : NULL;
		}
		else if(tmp == '\b')
		{
			if(c->cursor_x == 0 && c->cursor_y > old_y && i > 0)
			{
				c->cursor_y--;
				c->cursor_x = c->max_x;
			}
			if((c->cursor_x > old_x || (c->cursor_x > 0 && c->cursor_y > old_y )) && i > 0)
			{
				nio_cursor_erase(&cursor,c);
				nio_PrintStr(c,"\b \b");
				i--;
				str[i] = 0;
			}
		}
		else if(tmp == '\0')
		{
			str[0] = '\0';
			nio_PrintChar(c,'\n');
			return NULL;
		}
		else
		{
			str[i] = tmp;
			str[i+1] = 0;
			nio_PrintChar(c,tmp);
			nio_cursor_draw(&cursor,c);
			i++;
		}
	}
	str[num-1] = '\0';
	str[num-2] = '\n';
	
	nio_PrintChar(c,'\n');
	return str;
}

bool first_read = 1;
bool scroll_on_input = 0;

void show_initial_reverse();

/* Read one line, including the newline, into s. */
static void getline(char *s)
{
	if(prev_input_pos == 11){
		//we need to push all of the other entries back
		free(prev_input[0]);
		prev_input[0] = NULL;
		prev_input_pos--;
		
		int i;
		
		for(i = 1;i < 11;i++){
			prev_input[i-1] = prev_input[i];
		}
	}
	
	current_input_pos = prev_input_pos;
	
	if(first_read){
		first_read = 0;
		
		if(!scroll_on_input){
			show_initial_reverse();
		}
		else{
			scroll_on_input = 0;
			//if it's the first time getting input, we need to keep scrolling
			//up the screen until it has no blank lines at the top
			first_read = 0;
			bool keep_scrolling = 1;
			int i,d;
			int pos = 0;
			int count = 0;
			
			short cursor_x = console.cursor_x;
			short cursor_y = console.cursor_y;
			
			do{
				for(i = 0;i < console.max_x;i++){
					if(console.data[pos] != ' ' && console.data[pos] != 0){
						keep_scrolling = 0;
						break;
					}
					pos++;
				}
				
				if(keep_scrolling){
					count++;
				}
			} while(keep_scrolling && pos < console.max_x*console.max_y);
			
			for(i = 0;i < count;i++){
				/*for(d = 0;d < console.max_x;d++){
					console.data[console.cursor_y*console.max_x + d] = 0;
					console.color[console.cursor_y*console.max_x + d] = 0;
				}*/
				nio_printf(&console,"\n");
			}
			
			console.cursor_x = cursor_x;
			console.cursor_y = cursor_y-count;
			
			for(i = console.cursor_y*console.max_x + console.cursor_x;i < console.max_x*console.max_y;i++){
				console.data[i] = 0;
				console.color[i] = 0;//(background_color << 4) | text_color;
			}
		}
	}
			
			
    size_t length;
    nio_fgets_special(s,1000,&console);
    length = strlen(s);
    s[length] = '\n';
    s[length + 1] = '\0';
	
	prev_input[prev_input_pos] = malloc(1000);
	strcpy(prev_input[prev_input_pos],s);
	
	prev_input_pos++;
}

/* Translate in place all the escape characters in s.  */
static void translate_special_chars(char *s)
{
    char *src = s, *dest = s;
    while (*src)
        switch(*src++) {
            default: *dest++ = src[-1]; break;
            case '\n': *dest++ = ZC_RETURN; break;
            case '\\':
                switch (*src++) {
                    case '\n': *dest++ = ZC_RETURN; break;
                    case '\\': *dest++ = '\\'; break;
                    case '?': *dest++ = ZC_BACKSPACE; break;
                    case '[': *dest++ = ZC_ESCAPE; break;
                    case '_': *dest++ = ZC_RETURN; break;
                    case '^': *dest++ = ZC_ARROW_UP; break;
                    case '.': *dest++ = ZC_ARROW_DOWN; break;
                    case '<': *dest++ = ZC_ARROW_LEFT; break;
                    case '>': *dest++ = ZC_ARROW_RIGHT; break;
                    case 'R': *dest++ = ZC_HKEY_RECORD; break;
                    case 'P': *dest++ = ZC_HKEY_PLAYBACK; break;
                    case 'S': *dest++ = ZC_HKEY_SEED; break;
                    case 'U': *dest++ = ZC_HKEY_UNDO; break;
                    case 'N': *dest++ = ZC_HKEY_RESTART; break;
                    case 'X': *dest++ = ZC_HKEY_QUIT; break;
                    case 'D': *dest++ = ZC_HKEY_DEBUG; break;
                    case 'H': *dest++ = ZC_HKEY_HELP; break;
                    case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                        *dest++ = ZC_FKEY_MIN + src[-1] - '0' - 1; break;
                    case '0': *dest++ = ZC_FKEY_MIN + 9; break;
                    default:
                        PRINT_ALT("DUMB-FROTZ: unknown escape char: %c\n", src[-1]);
                        PRINT_ALT("Enter \\help to see the list\n");
                }
        }
    *dest = '\0';
}


/* The time in tenths of seconds that the user is ahead of z time.  */
static int time_ahead = 0;

/* Called from os_read_key and os_read_line if they have input from
 * a previous call to dumb_read_line.
 * Returns TRUE if we should timeout rather than use the read-ahead.
 * (because the user is further ahead than the timeout).  */
static bool check_timeout(int timeout)
{
    if ((timeout == 0) || (timeout > time_ahead))
        time_ahead = 0;
    else
        time_ahead -= timeout;
    return time_ahead != 0;
}

/* If val is '0' or '1', set *var accordingly, otherwise toggle it.  */
static void toggle(bool *var, char val)
{
    *var = val == '1' || (val != '0' && !*var);
}

/* Handle input-related user settings and call dumb_output_handle_setting.  */
bool dumb_handle_setting(const char *setting, bool show_cursor, bool startup)
{
    if (!strncmp(setting, "sf", 2)) {
        speed = atof(&setting[2]);
        PRINT("Speed Factor %g\n", speed);
    } else if (!strncmp(setting, "mp", 2)) {
        toggle(&do_more_prompts, setting[2]);
        PRINT("More prompts %s\n", do_more_prompts ? "ON" : "OFF");
    } else {
        if (!strcmp(setting, "set")) {
            PRINT("Speed Factor %g\n", speed);
            PRINT("More Prompts %s\n", do_more_prompts ? "ON" : "OFF");
        }
        return dumb_output_handle_setting(setting, show_cursor, startup);
    }
    return TRUE;
}

/* Read a line, processing commands (lines that start with a backslash
 * (that isn't the start of a special character)), and write the
 * first non-command to s.
 * Return true if timed-out.  */
static bool dumb_read_line(char *s, char *prompt, bool show_cursor,
                           int timeout, enum input_type type,
                           zchar *continued_line_chars)
{
    time_t start_time;
	//show_cursor = 0;

    if (timeout) {
        if (time_ahead >= timeout) {
            time_ahead -= timeout;
            return TRUE;
        }
        timeout -= time_ahead;
        start_time = RTC();
    }
    time_ahead = 0;
	
	//printf("prompt = %s\n",prompt);

    dumb_show_screen(show_cursor);
	//wait_key_pressed();
    for (;;) {
        char *command;
        if (prompt)
            PRINT(prompt);
        else
            dumb_show_prompt(show_cursor, (timeout ? "tTD" : ")>}")[type]);
		
		//wait_key_pressed();
        getline(s);
        if ((s[0] != '\\') || ((s[1] != '\0') && !islower(s[1]))) {
            /* Is not a command line.  */
            translate_special_chars(s);
            if (timeout) {
                int elapsed = (RTC() - start_time) * 10 * speed;
                if (elapsed > timeout) {
                    time_ahead = elapsed - timeout;
                    return TRUE;
                }
            }
            return FALSE;
        }
        /* Commands.  */

        /* Remove the \ and the terminating newline.  */
        command = s + 1;
        command[strlen(command) - 1] = '\0';

        if (!strcmp(command, "t")) {
            if (timeout) {
                time_ahead = 0;
                s[0] = '\0';
                return TRUE;
            }
        } else if (*command == 'w') {
            if (timeout) {
                int elapsed = atoi(&command[1]);
                time_t now = RTC();
                if (elapsed == 0)
                    elapsed = (now - start_time) * 10 * speed;
                if (elapsed >= timeout) {
                    time_ahead = elapsed - timeout;
                    s[0] = '\0';
                    return TRUE;
                }
                timeout -= elapsed;
                start_time = now;
            }
        } else if (!strcmp(command, "d")) {
            if (type != INPUT_LINE_CONTINUED)
                PRINT_ALT("DUMB-FROTZ: No input to discard\n");
            else {
                dumb_discard_old_input(strlen(continued_line_chars));
                continued_line_chars[0] = '\0';
                type = INPUT_LINE;
            }
        } else if (!strcmp(command, "help")) {
            if (!do_more_prompts)
                PRINT(runtime_usage);
            else {
                char *current_page, *next_page;
                current_page = next_page = runtime_usage;
                for (;;) {
                    int i;
                    for (i = 0; (i < h_screen_rows - 2) && *next_page; i++)
                        next_page = strchr(next_page, '\n') + 1;
                    PRINT("%.*s", next_page - current_page, current_page);
                    current_page = next_page;
                    if (!*current_page)
                        break;
                    PRINT("HELP: Type <return> for more, or q <return> to stop: ");
                    getline(s);
                    if (!strcmp(s, "q\n"))
                        break;
                }
            }
        } else if (!strcmp(command, "s")) {
            dumb_dump_screen();
        } else if (!dumb_handle_setting(command, show_cursor, FALSE)) {
            PRINT_ALT("DUMB-FROTZ: unknown command: %s\n", s);
            PRINT_ALT("Enter \\help to see the list of commands\n");
        }
    }
}

/* Read a line that is not part of z-machine input (more prompts and
 * filename requests).  */
static void dumb_read_misc_line(char *s, char *prompt)
{
    dumb_read_line(s, prompt, 0, 0, 0, 0);
    /* Remove terminating newline */
    s[strlen(s) - 1] = '\0';
}

/* For allowing the user to input in a single line keys to be returned
 * for several consecutive calls to read_char, with no screen update
 * in between.  Useful for traversing menus.  */
static char read_key_buffer[INPUT_BUFFER_SIZE];

/* Similar.  Useful for using function key abbreviations.  */
static char read_line_buffer[INPUT_BUFFER_SIZE];

zchar os_read_key (int timeout, bool show_cursor)
{
    char c;
    int timed_out;

    /* Discard any keys read for line input.  */
    read_line_buffer[0] = '\0';

    if (read_key_buffer[0] == '\0') {
        timed_out = dumb_read_line(read_key_buffer, NULL, show_cursor, timeout,
                                   INPUT_CHAR, NULL);
    /* An empty input line is reported as a single CR.
     * If there's anything else in the line, we report only the line's
     * contents and not the terminating CR.  */
        if (strlen(read_key_buffer) > 1)
            read_key_buffer[strlen(read_key_buffer) - 1] = '\0';
    } else
        timed_out = check_timeout(timeout);

    if (timed_out)
        return ZC_TIME_OUT;

    c = read_key_buffer[0];
    memmove(read_key_buffer, read_key_buffer + 1, strlen(read_key_buffer));

    /* TODO: error messages for invalid special chars.  */

    return c;
}

zchar os_read_line (int max, zchar *buf, int timeout, int width, int continued)
{
    char *p;
    int terminator;
    static bool timed_out_last_time;
    int timed_out;

    /* Discard any keys read for single key input.  */
    read_key_buffer[0] = '\0';

    /* After timing out, discard any further input unless we're continuing.  */
    if (timed_out_last_time && !continued)
        read_line_buffer[0] = '\0';

    if (read_line_buffer[0] == '\0')
        timed_out = dumb_read_line(read_line_buffer, NULL, TRUE, timeout,
                                   buf[0] ? INPUT_LINE_CONTINUED : INPUT_LINE,
                                   buf);
    else
        timed_out = check_timeout(timeout);

    if (timed_out) {
        timed_out_last_time = TRUE;
        return ZC_TIME_OUT;
    }

    /* find the terminating character.  */
    for (p = read_line_buffer;; p++) {
        if (is_terminator(*p)) {
            terminator = *p;
            *p++ = '\0';
            break;
        }
    }

    /* TODO: Truncate to width and max.  */

    /* copy to screen */
    dumb_display_user_input(read_line_buffer);

    /* copy to the buffer and save the rest for next time.  */
    strcat(buf, read_line_buffer);
    p = read_line_buffer + strlen(read_line_buffer) + 1;
    memmove(read_line_buffer, p, strlen(p) + 1);

    /* If there was just a newline after the terminating character,
     * don't save it.  */
    if ((read_line_buffer[0] == '\r') && (read_line_buffer[1] == '\0'))
        read_line_buffer[0] = '\0';

    timed_out_last_time = FALSE;
    return terminator;
}

int os_read_file_name (char *file_name, const char *default_name, int flag)
{
    char buf[INPUT_BUFFER_SIZE], prompt[INPUT_BUFFER_SIZE];
    FILE *fp;

    sprintf(prompt, "Please enter a filename [%s]: ", default_name);
    dumb_read_misc_line(buf, prompt);
    if (strlen(buf) > MAX_FILE_NAME) {
        PRINT("Filename too long\n");
        return FALSE;
    }

    strcpy (file_name, buf[0] ? buf : default_name);

    /* Warn if overwriting a file.  */
    if ((flag == FILE_SAVE || flag == FILE_SAVE_AUX || flag == FILE_RECORD)
    && ((fp = fopen(file_name, "rb")) != NULL)) {
        fclose (fp);
        dumb_read_misc_line(buf, "Overwrite existing file? ");
        return(tolower(buf[0]) == 'y');
    }
    return TRUE;
}

void os_more_prompt (void)
{
    if (do_more_prompts) {
        char buf[INPUT_BUFFER_SIZE];
        dumb_read_misc_line(buf, "***MORE***");
    } else
        dumb_elide_more_prompt();
}

void dumb_init_input(void)
{
    if ((h_version >= V4) && (speed != 0))
        h_config |= CONFIG_TIMEDINPUT;

    if (h_version >= V5)
        h_flags &= ~(MOUSE_FLAG | MENU_FLAG);
}

zword os_read_mouse(void)
{
    /* NOT IMPLEMENTED */
}
