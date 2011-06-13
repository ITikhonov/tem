#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>



int cursor;

char viewbuf[80*20];

static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	cairo_t *cr = gdk_cairo_create (widget->window);
	PangoLayout *p=pango_cairo_create_layout(cr);
	PangoFontDescription *desc=pango_font_description_from_string("Courier 12");
	pango_layout_set_font_description(p,desc);
	pango_font_description_free(desc);

	int w,h;
	pango_layout_set_text(p,"0",1);
	pango_layout_get_size(p,&w,&h);
	w/=PANGO_SCALE;
	h/=PANGO_SCALE;

	if(cursor<20*80 && cursor>=0) {
		cairo_save(cr);
		cairo_set_source_rgb(cr,0,0.8,0);
		cairo_rectangle(cr,(cursor%80+1)*w,(cursor/80+1)*h,w,h);
		cairo_fill(cr);
		cairo_restore(cr);
	}

	cairo_move_to(cr,w,h);

	int i;
	for(i=0;i<20;i++) {
		pango_layout_set_text(p,viewbuf+80*i,80);
		pango_cairo_update_layout(cr,p);
		pango_cairo_show_layout(cr,p);
		cairo_rel_move_to(cr,0,h);
	}

	cairo_move_to(cr,w*81+w/2,h);
	cairo_rel_line_to(cr,0,h*20);
	cairo_line_to(cr,w,h*21);
	cairo_stroke(cr);

	g_object_unref(p);


	cairo_destroy(cr);
	return FALSE;
}


static gboolean on_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	printf("pressed %s\n",event->string);

	switch(event->keyval) {
	case GDK_Escape: gtk_main_quit(); break;
	case GDK_BackSpace: cursor--; cursor%=(80*20); viewbuf[cursor]=' '; break;
	case GDK_Left: cursor--; break;
	case GDK_Right: cursor++; break;
	case GDK_Up: cursor-=80; break;
	case GDK_Down: cursor+=80; break;
	case GDK_Return: cursor=(cursor/80+1)*80;; break;
	default:
		if(event->length==1) {
			viewbuf[cursor++]=event->string[0];
		}
	}

	printf("%d --\n",cursor);
	if(cursor<0) cursor=80*20+cursor;
	else cursor%=(80*20);
	printf("-- %d\n",cursor);

	gdk_window_invalidate_rect(gtk_widget_get_window(widget),0,1);

	return FALSE;
}

int main(int argc,char *argv[])
{
	int i;
	for(i=0;i<80*20;i++) {viewbuf[i]=' ';}

	gtk_init(&argc,&argv);
	GtkWidget *window=gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window),"te");

	g_signal_connect(window,"destroy",G_CALLBACK (gtk_main_quit),NULL);
	g_signal_connect(window,"key-press-event",G_CALLBACK(on_keypress),NULL);
	gtk_widget_add_events(window,GDK_KEY_PRESS_MASK);

	GtkWidget *a=gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window),a);
	g_signal_connect(a,"expose-event",G_CALLBACK(on_expose_event),NULL);
	gtk_widget_show_all(window);

	gtk_main();
	
	return 0;
}




