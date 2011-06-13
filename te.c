#include <gtk/gtk.h>
#include <stdio.h>

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

	cairo_save(cr);
	cairo_set_source_rgb(cr,1,1,1);
	cairo_rectangle(cr,w*5,h*5,w,h);
	cairo_fill(cr);
	cairo_restore(cr);

	cairo_move_to(cr,w,h);

	int i;
	for(i=0;i<20;i++) {
		pango_layout_set_text(p,viewbuf+80*i,80);
		pango_cairo_update_layout(cr,p);
		pango_cairo_show_layout(cr,p);
		cairo_rel_move_to(cr,0,h);
	}

	g_object_unref(p);


	cairo_destroy(cr);
	return FALSE;
}

static gboolean on_keypress(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	printf("pressed\n");
	gtk_main_quit();
	return FALSE;
}

int main(int argc,char *argv[])
{
	int i;
	for(i=0;i<80*20;i++) {viewbuf[i]='A'+(i%25);}

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




