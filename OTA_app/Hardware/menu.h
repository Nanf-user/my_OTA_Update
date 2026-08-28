#ifndef __MENU_H
#define __MENU_H

typedef struct MenuItem
{
    char name[20];

    struct MenuItem *parent;
    struct MenuItem *child;
    struct MenuItem *prev;
	  struct MenuItem *next;
		void (*action)(void);

}MenuItem;

static MenuItem *current;
static MenuItem *currentList;

void Menu_Init(void);
void Menu_operate(uint8_t key);
void Menu_Display(void);


#endif
