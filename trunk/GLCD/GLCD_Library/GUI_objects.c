/**
* \file GUI_objects.c
* \brief BRTOS LCD Graphical Objects
*
* Author: Gustavo Weber Denardin and Miguel Moreto
*
*
**/

#include "GUI_objects.h"
#include "touch_7846.h"

/* Declare object linked list head and tail */
ObjectEvent_typedef *ObjectHead;
ObjectEvent_typedef *ObjectTail;

/* Declare touchscreen events list */
BRTOS_Queue	*TouchEvents;

/* Declare touchscreen events signaling semaphore */
BRTOS_Sem 	*TouchSync;

/* Declare background color */
color_t GuiBackground;

/* Declare Button functions structure */
ButtonFunc_typedef Button;

/* Declare Slider functions structure */
SliderFunc_typedef Slider;

/*  Initialization of the functions structures */
void GUI_ObjetcsInit(color_t background)
{
#if (USE_BUTTON == TRUE)
	Button.Draw = Button_Draw;
	Button.Init = Button_Init;
	Button.Update = Button_Update;
	Button.Click = Button_Click;
#endif

#if (USE_SLIDER == TRUE)
	Slider.Draw = Slider_Draw;
	Slider.Init = Slider_Init;
	Slider.Update = Slider_Update;
	Slider.Click = Slider_Click;
#endif
	GuiBackground = background;
}


/* Declare function used to include objects into the object event list */
void GUI_IncludeObjectIntoEventList(ObjectEvent_typedef *object)
{
	if (ObjectTail != NULL)
	{
		ObjectTail->Next = object;
		object->Previous = ObjectTail;
		ObjectTail = object;
		ObjectTail->Next = NULL;
	}
	else
	{
		ObjectTail = object;
		ObjectHead = object;
		object->Next = NULL;
		object->Previous = NULL;
	}
}


/* Declare function used to remove objects from the object event list */
void GUI_RemoveObjectIntoEventList(ObjectEvent_typedef *object)
{
	if (object == ObjectHead)
	{
		if (object == ObjectTail)
		{
			ObjectTail = NULL;
			ObjectHead = NULL;
		}else
		{
			ObjectHead = object->Next;
			ObjectHead->Previous = NULL;
		}
	}else
	{
		if (object == ObjectTail)
		{
			ObjectTail = object->Previous;
			ObjectTail->Next = NULL;
		}else
		{
			object->Next->Previous = object->Previous;
			object->Previous->Next = object->Next;
		}
	}
}



/* Declare function used to verify objects inside touched region */
ObjectEvent_typedef *GUI_VerifyObjects(int x, int y)
{
	ObjectEvent_typedef *object = ObjectHead;

	  ////////////////////////////////////////////////////
	  // Search for inside object 						//
	  ////////////////////////////////////////////////////
	  while(object != NULL)
	  {
		  if((x >= (object->x1)) && (x <= (object->x2)) && (y >= (object->y1)) && (y <= (object->y2)))
		  {
			  return object;
		  }else
		  {
			  object = object->Next;
		  }
	  }
	  return NULL;
}


/* Declare GUI event handler task */
void GUI_Event_Handler(void *param)
{
	Touch_typedef TouchPos;
	ObjectEvent_typedef *object;
	Button_typedef		*button;
	Slider_typedef		*slider;
	TouchPos.x = 0;
	TouchPos.y = 0;

	(void)OSDQueueCreate(1024, sizeof(Callbacks), &TouchEvents);
	(void)OSSemCreate(0,&TouchSync);

	while(1)
	{
		// Wait for a touch in the screen
		(void)OSSemPend(TouchSync,0);

		// Read touch position
		TP_Read();

		// If there is touch in a different place
		// !!!!!!!!!!!! Lembrar que esse if � interessante para uma tela de paint
		//if ((TouchPos.x != Pen_Point.X0) || (TouchPos.y != Pen_Point.Y0))
		//{
			TouchPos.x = Pen_Point.X0;
			TouchPos.y = Pen_Point.Y0;

			object = GUI_VerifyObjects(TouchPos.x, TouchPos.y);
			if (object != FALSE)
			{
				switch(object->object)
				{
					case BUTTON_OBJECT :
						button = (Button_typedef*)object->ObjectPointer;
						Button.Click(button);
						while(GPIO_ReadInputDataBit(TOUCH_INT_PORT,TOUCH_INT_PIN)==0)
						{
							DelayTask(10);
						}
						OSDQueuePost(TouchEvents, (void*)(&(button->ClickEvent)));
						Button.Draw(button);
					break;

					case SLIDER_OBJECT :
						slider = (Slider_typedef*)object->ObjectPointer;
						int slider_size = (slider->x2) - (slider->x1);
						while(GPIO_ReadInputDataBit(TOUCH_INT_PORT,TOUCH_INT_PIN)==0)
						{
							DelayTask(20);

							// Read touch position
							TP_Read();
							TouchPos.x = Pen_Point.X0;
							TouchPos.y = Pen_Point.Y0;

							// Verify if still over slider
							if (object == GUI_VerifyObjects(TouchPos.x, TouchPos.y))
							{
								// Calcule new value
								slider->value = (((TouchPos.x) - (slider->x1) - 7) * 100) / slider_size;

								if (slider->value <0) slider->value = 0;
								if (slider->value >100) slider->value = 100;

								// Draw slider bar
								Slider.Click(slider);
							}
						}
						OSDQueuePost(TouchEvents, (void*)(&(slider->ClickEvent)));
					break;

					/* Unknown object */
					default:
					break;
				}
			}
		//}
		DelayTask(3);

	    // Enable interrupt again.
	    if(EXTI_GetITStatus(TOUCH_EXTI_Line) != RESET)
	    {
	    	EXTI_ClearITPendingBit(TOUCH_EXTI_Line);
	    }
	    TP_InterruptEnable(ENABLE);
	}
}

/* Touchscreen interrupt handler */
void TP_Handler(void){

	(void)OSSemPost(TouchSync);

	/* Disable TP interrupt: */
	TP_InterruptEnable(DISABLE);
}




#if (USE_BUTTON == TRUE)
/* Initializes the Button structure */
void Button_Init(coord_t x, coord_t y, coord_t width, coord_t height,
		coord_t radius, color_t bg_color, color_t font_color, char *str,
		Button_typedef *Button_struct, Callbacks click_event)
{
	Button_struct->x = x;
	Button_struct->y = y;
	Button_struct->dx = width;
	Button_struct->dy = height;
	Button_struct->radius = radius;
	Button_struct->bg_color = bg_color;
	Button_struct->font_color = font_color;
	Button_struct->str = str;

	// Event handler
	Button_struct->event.x1 = x+4;
	Button_struct->event.y1 = y+4;
	Button_struct->event.x2 = x+width-8;
	Button_struct->event.y2 = y+height-8;

	GUI_IncludeObjectIntoEventList(&(Button_struct->event));

	Button_struct->event.object = BUTTON_OBJECT;
	Button_struct->event.ObjectPointer = (void*)Button_struct;
	Button_struct->ClickEvent = click_event;
}


/* Initializes the Button structure */
void Button_Update(coord_t x, coord_t y, coord_t width, coord_t height,
		coord_t radius, color_t bg_color, color_t font_color, char *str,
		Button_typedef *Button_struct)
{
	Button_struct->x = x;
	Button_struct->y = y;
	Button_struct->dx = width;
	Button_struct->dy = height;
	Button_struct->radius = radius;
	Button_struct->bg_color = bg_color;
	Button_struct->font_color = font_color;
	Button_struct->str = str;
}

/* Function to draw a box with rounded corners */
void Button_Draw(Button_typedef *Button_struct)
{
	coord_t x1, x2, y1, y2;
	coord_t size_x, size_y;

	x1 = Button_struct->x + Button_struct->radius;
	x2 = Button_struct->x + Button_struct->dx - Button_struct->radius;
	y1 = Button_struct->y + Button_struct->radius;
	y2 = Button_struct->y + Button_struct->dy - Button_struct->radius;

	// Draw the corners
	gdispFillCircle(x1,y1,Button_struct->radius,Button_struct->bg_color);
	gdispFillCircle(x2,y1,Button_struct->radius,Button_struct->bg_color);
	gdispFillCircle(x1,y2,Button_struct->radius,Button_struct->bg_color);
	gdispFillCircle(x2,y2,Button_struct->radius,Button_struct->bg_color);

	// Draw the inner rectangles.
	gdispFillArea(Button_struct->x,y1,Button_struct->radius,Button_struct->dy-2*Button_struct->radius,Button_struct->bg_color);
	gdispFillArea(x2,y1,Button_struct->radius+1,Button_struct->dy-2*Button_struct->radius,Button_struct->bg_color);
	gdispFillArea(x1,Button_struct->y,Button_struct->dx-2*Button_struct->radius,Button_struct->dy+1,Button_struct->bg_color);
	// Obtaining the size of the text.
	size_x = gdispGetStringWidth(Button_struct->str, &fontLarger);
	size_y = gdispGetFontMetric(&fontLarger, fontHeight) -
			gdispGetFontMetric(&fontLarger, fontDescendersHeight);

	// Draw the string in the middle of the box.
	gdispDrawString(Button_struct->x+(Button_struct->dx-size_x)/2,
			Button_struct->y+(Button_struct->dy-size_y)/2,
			Button_struct->str, &fontLarger, Button_struct->font_color);

}


/* Function to draw a box with rounded corners */
void Button_Click(Button_typedef *Button_struct)
{
	  Button_typedef Button_backup;
	  Button_backup = *Button_struct;

	  // Linha de cima
	  gdispFillArea(Button_struct->x,Button_struct->y,Button_struct->dx,2,GuiBackground);
	  //Linha da esquerda
	  gdispFillArea(Button_struct->x,Button_struct->y,2,Button_struct->dy,GuiBackground);
	  //Quadrado do canto superior esquerdo
	  gdispFillArea(Button_struct->x,Button_struct->y,Button_struct->radius,Button_struct->radius,GuiBackground);
	  //Quadrado do canto inferior esquerdo
	  gdispFillArea(Button_struct->x,(Button_struct->y)+(Button_struct->dy)-12, Button_struct->radius,Button_struct->radius,GuiBackground);
	  // Linha de baixo
	  gdispFillArea(Button_struct->x,(Button_struct->y)+(Button_struct->dy)-2,Button_struct->dx,4,GuiBackground);
	  // Linha da direita
	  gdispFillArea((Button_struct->x)+ (Button_struct->dx)-2,Button_struct->y,3,Button_struct->dy,GuiBackground);
	  //Quadrado do canto superior direito
	  gdispFillArea((Button_struct->x)+(Button_struct->dx)-12,Button_struct->y,Button_struct->radius,Button_struct->radius,GuiBackground);
	  //Quadrado do canto inferior esquerdo
	  gdispFillArea((Button_struct->x)+(Button_struct->dx)-12,(Button_struct->y)+(Button_struct->dy)-12,Button_struct->radius,Button_struct->radius,GuiBackground);

	  // Draw pressed button
	  Button.Update((coord_t)((Button_struct->x)+2),(coord_t)((Button_struct->y)+2),(coord_t)((Button_struct->dx)-4),
			  (coord_t)((Button_struct->dy)-4),(Button_struct->radius),(Button_struct->bg_color),(color_t)0x7BEF,
			  (Button_struct->str),&Button_backup);
	  Button.Draw(&Button_backup);
}


#endif




#if (USE_SLIDER == TRUE)
/* Initializes the Button structure */
void Slider_Init(coord_t x, coord_t y, coord_t width, coord_t height,
		color_t border_color, color_t fg_color, int value,
		Slider_typedef *Slider_struct, Callbacks click_event)
{
	Slider_struct->x = x;
	Slider_struct->y = y;
	Slider_struct->dx = width;
	Slider_struct->dy = height;
	Slider_struct->radius = 4;
	Slider_struct->border_color = border_color;
	Slider_struct->fg_color = fg_color;
	Slider_struct->value = value;

	// Slider bar area
	Slider_struct->x1 = (Slider_struct->x)+4;
	Slider_struct->y1 = (Slider_struct->y)+4;
	Slider_struct->x2 = (Slider_struct->dx)-7;
	Slider_struct->y2 = (Slider_struct->dy)-7;

	// Event handler
	Slider_struct->event.x1 = x+4;
	Slider_struct->event.y1 = y+4;
	Slider_struct->event.x2 = x+width-8;
	Slider_struct->event.y2 = y+height-6;

	GUI_IncludeObjectIntoEventList(&(Slider_struct->event));

	Slider_struct->event.object = SLIDER_OBJECT;
	Slider_struct->event.ObjectPointer = (void*)Slider_struct;
	Slider_struct->ClickEvent = click_event;
}


/* Initializes the Button structure */
void Slider_Update(coord_t x, coord_t y, coord_t width, coord_t height,
		color_t border_color, color_t fg_color, int value,
		Slider_typedef *Slider_struct)
{
	Slider_struct->x = x;
	Slider_struct->y = y;
	Slider_struct->dx = width;
	Slider_struct->dy = height;
	Slider_struct->border_color = border_color;
	Slider_struct->fg_color = fg_color;
	Slider_struct->value = value;
}

/* Function to draw a box with rounded corners */
void Slider_Draw(Slider_typedef *Slider_struct)
{
	coord_t x1, x2, y1, y2;

	x1 = Slider_struct->x + Slider_struct->radius;
	x2 = Slider_struct->x + Slider_struct->dx - Slider_struct->radius;
	y1 = Slider_struct->y + Slider_struct->radius;
	y2 = Slider_struct->y + Slider_struct->dy - Slider_struct->radius;

	// Draw the corners
	gdispFillCircle(x1,y1,Slider_struct->radius,Slider_struct->border_color);
	gdispFillCircle(x2,y1,Slider_struct->radius,Slider_struct->border_color);
	gdispFillCircle(x1,y2,Slider_struct->radius,Slider_struct->border_color);
	gdispFillCircle(x2,y2,Slider_struct->radius,Slider_struct->border_color);

	// Draw the inner rectangles.
	gdispFillArea(Slider_struct->x,y1,Slider_struct->radius,Slider_struct->dy-2*Slider_struct->radius,Slider_struct->border_color);
	gdispFillArea(x2,y1,Slider_struct->radius+1,Slider_struct->dy-2*Slider_struct->radius,Slider_struct->border_color);
	gdispFillArea(x1,Slider_struct->y,Slider_struct->dx-2*Slider_struct->radius,Slider_struct->dy+1,Slider_struct->border_color);

	// Draw background border
	gdispFillArea(Slider_struct->x+2,Slider_struct->y+2,(Slider_struct->dx)-3,(Slider_struct->dy)-3,GuiBackground);

	// Draw slider bar
	gdispFillArea(Slider_struct->x1,Slider_struct->y1,((Slider_struct->x2)*(Slider_struct->value))/100,Slider_struct->y2,Slider_struct->fg_color);

	Slider_struct->lvalue = ((Slider_struct->x2)*(Slider_struct->value))/100;
}


/* Function to draw a box with rounded corners */
void Slider_Click(Slider_typedef *Slider_struct)
{
	  	coord_t update;

		update = ((Slider_struct->x2)*(Slider_struct->value))/100;

		// Draw background border
		if (update < Slider_struct->lvalue)
		{
			//gdispFillArea(Slider_struct->x+2,Slider_struct->y+2,(Slider_struct->dx)-3,(Slider_struct->dy)-3,GuiBackground);
			gdispFillArea(update+(Slider_struct->x1),(Slider_struct->y)+2,(Slider_struct->lvalue)-update,(Slider_struct->dy)-3,GuiBackground);
		}

		// Draw slider bar
		if (update > Slider_struct->lvalue)
		{
			gdispFillArea((Slider_struct->lvalue)+(Slider_struct->x1),Slider_struct->y1,update-(Slider_struct->lvalue),Slider_struct->y2,Slider_struct->fg_color);
		}
		Slider_struct->lvalue = update;
}

#endif
