#include <gtk/gtk.h>
#include <stdio.h>

static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	cairo_t *cr = gdk_cairo_create (widget->window);
	cairo_move_to(cr, 40, 50);  
	cairo_line_to(cr, 200, 50);
	cairo_stroke(cr);
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




