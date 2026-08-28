#include "stm32f10x.h"
#include "menu.h"
#include "string.h"
#include "mylcd.h"
#include "stdio.h"
#include <stdlib.h>
#include "LED.h"

uint8_t i = 0;
	
MenuItem* Menu_Create(char *name, void (*action)(void))
{
		MenuItem *node = (MenuItem *)malloc(sizeof(MenuItem));
		if(NULL == node) 
		return NULL;
		strncpy(node->name,name,sizeof(node->name));
		node->parent = NULL;
		node->child = NULL;
		node->prev = NULL;
		node->next = NULL;
		node->action = action; 
		return node;
	
}
void Menu_AddChild(MenuItem *parent, MenuItem *child)
{
    child->parent = parent;

    if(parent->child == NULL)
    {
        parent->child = child;
    }
    else
    {
        MenuItem *p = parent->child;
        while(p->next) p = p->next;
        p->next = child;
			  child->prev = p;
    }
}

void Func_LED(void)
{
    LCD_ShowString(0,200,"LED",RED,WHITE,32,0);
}

void Func_Motor(void)
{
    LCD_ShowString(0,200,"Motor",RED,WHITE,32,0);
}
void Func_LED_on(void)
{		
		LCD_ShowString(0,200,"ON",RED,WHITE,32,0);  //  调试
		LED_1_On();
}
void Func_LED_off(void)
{		
		LCD_ShowString(0,200,"OFF",RED,WHITE,32,0);  //  调试
		LED_1_Off();
}
void Menu_Init(void)
{
    MenuItem *main = Menu_Create("菜单", NULL);

    MenuItem *setting = Menu_Create("test1", NULL);
    MenuItem *info    = Menu_Create("test2", NULL);
	  MenuItem *test    = Menu_Create("设置", NULL);


    MenuItem *led     = Menu_Create("led", Func_LED);
    MenuItem *motor   = Menu_Create("motor", Func_Motor);
	
		MenuItem *light  = Menu_Create("light", NULL);
		MenuItem *open  = Menu_Create("open", Func_LED_on);
		MenuItem *off  = Menu_Create("off", Func_LED_off);

    /* 一行一行加 */
	
    Menu_AddChild(main, setting);
    Menu_AddChild(main, info);
		Menu_AddChild(main, test);

    Menu_AddChild(setting, led);
    Menu_AddChild(setting, motor);
		
		Menu_AddChild(test,light);
		Menu_AddChild(light,open);
		Menu_AddChild(light,off);

	

    /* 当前状态 */
    current = main->child;
    currentList = main->child;
		

}

void Menu_operate(uint8_t key)
{
	switch(key)
	{
		case 1:         //向上
		if(current->prev)
    current = current->prev;
		else
			{
		    MenuItem *p = current;
        while(p->next)
        p = p->next;
				current = p;
			}
    Menu_Display();   
		break;
		
		case 2:        //向下
    if(current->next)
    current = current->next;
		else
    {
        // 跳到第一个
        MenuItem *p = current;
        while(p->prev)
            p = p->prev;

        current = p;
    }
		Menu_Display();	
		break;
			
		case 3:       //确认
		if(current->action)
    {
        current->action();
    }
    else if(current->child)
    {
        current = current->child;
        currentList = current;
    }

		Menu_Display();
		break;
		
		case 4:              //返回
    if(current->parent)
    {
        MenuItem *parent = current->parent;

        current = parent;

        if(parent->parent)
            currentList = parent->parent->child;
        else
            currentList = parent;  // 根节点
    }
		Menu_Display();
		break;
	
	}


}

void Menu_Display(void)
{
    static MenuItem *last = NULL;

    if(last == current) return; // 防抖

    last = current;

    ST7789_Clear(WHITE);

    MenuItem *p = currentList;
    uint8_t i = 1;

    while(p)
    {
        if(p == current)
        LCD_ShowString(0, i*40, ">",RED,WHITE,32,0);
				LCD_ShowChinese(80, 10, (uint8_t*)p->parent->name,RED,WHITE,32,0);
				
				if(*p->name & 0x80)  //判断是否是中文
				{
						LCD_ShowChinese(20, i*40, (uint8_t*)p->name,RED,WHITE,32,0);
				}
				else
				{
						LCD_ShowString(20, i*40, (uint8_t*)p->name,RED,WHITE,32,0);	
				}
        p = p->next;
        i++;

        if(i >= 4) break;
    }
}
